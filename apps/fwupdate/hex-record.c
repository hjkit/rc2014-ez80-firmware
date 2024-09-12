#pragma printf = "%0X %X %x %s %c %u %f %d %u %ld %lld %llu %lu"

#include "hex-record.h"
#include <ifl.h>
#include <stdio.h>

#define getch getchar

#define REC_TYPE_DATA              0x00
#define REC_TYPE_EOF               0x01
#define REC_TYPE_EXT_SEG_ADDR      0x02
#define REC_TYPE_START_SEG_ADDR    0x03
#define REC_TYPE_EXT_LINEAR_ADDR   0x04
#define REC_TYPE_START_LINEAR_ADDR 0x05

static uint8_t  length;
static uint8_t  check_sum;
static uint8_t  offset_high, offset_low;
static uint32_t base_addr = 0;

static uint8_t parse_byte(const uint8_t *ch) {
  static uint8_t high_nibble, low_nibble;

  high_nibble = ch[0] - '0';
  if (high_nibble > 9)
    high_nibble -= 7;

  low_nibble = ch[1] - '0';
  if (low_nibble > 9)
    low_nibble -= 7;

  return ((high_nibble << 4) | low_nibble);
}

static uint8_t get_next_byte(FILE *file) {
  static uint8_t ch[2];

  fread(ch, 2, 1, file);
  return parse_byte(ch);
}

static void find_record_start(FILE *file) __z88dk_fastcall {
  static uint8_t data;
  do {
    data = fgetc(file);
  } while (data != ':');
}

static uint8_t process_hex_record_header(FILE *file) __z88dk_fastcall {
  static uint8_t ch[2 * 4];
  static uint8_t record_type;

  find_record_start(file);

  fread(ch, 2, 4, file);

  length      = parse_byte(&ch[0]);
  offset_high = parse_byte(&ch[2]);
  offset_low  = parse_byte(&ch[4]);
  record_type = parse_byte(&ch[6]);
  check_sum   = length + offset_high + offset_low + record_type;

  return record_type;
}

static int8_t process_hex_record_eof(FILE *file) {
  check_sum += get_next_byte(file);
  return (check_sum != 0) ? ZFL_ERR_FAILURE : ZFL_ERR_SUCCESS;
}

static void process_hex_record_ext_seg_addr(FILE *file) {
  printf("\r\nUnexpected 16-bit Extended Segment Address Record");
  base_addr = (get_next_byte(file) << 12) + (get_next_byte(file) << 4);
}

static void process_hex_record_start_seg_addr(FILE *file) {
  get_next_byte(file);
  get_next_byte(file);
  printf("\r\nIgnore start Segment Address Record");
}

static int8_t process_hex_record_ext_linear_addr(FILE *file) {
  if (get_next_byte(file) == 0) {
    base_addr = ((uint32_t)get_next_byte(file) << 16);
    printf("\r\nbase_addr: %lX", base_addr);
    return ZFL_ERR_SUCCESS;
  }

  printf("\r\nInvalid Address");
  return ZFL_ERR_ADDRESS;
}

static void process_hex_record_start_linear_addr(FILE *file) {
  get_next_byte(file);
  get_next_byte(file);
  get_next_byte(file);
  get_next_byte(file);
  printf("\r\nIgnore start Linear Address Record");
}

int8_t process_hex_record_data(FILE *file, emit_func_t emit) {
  static int8_t   status = ZFL_ERR_SUCCESS;
  static uint8_t *pCh;
  static uint8_t *pBuffer;
  static uint16_t buffer_index;
  static uint8_t  buffer_ch[64 * 2 + 10];
  static uint8_t  buffer[64 + 10];
  static uint32_t offset;
  static uint8_t  data;

  offset = base_addr | (offset_high << 8) | offset_low;

  if (offset > IFL_ALT_BIOS_END) {
    // The hex files generated by ZDS also include BSS zeroed data
    // not something that can be flash, nor do we want to reset on-chip state
    // so we just ignore it
    printf("\r\nIgnoring data record: %lX...%lx", offset, offset);
    return ZFL_ERR_SUCCESS;
  }

  status = IFL_IsAddrValid(offset, length);
  if (status != ZFL_ERR_SUCCESS) {
    printf("\r\nIFL_IsAddrValid error: %X (%lX...%lx)", status, offset, offset + length);
    return status;
  }

  if (length > 64) {
    printf("\r\nData record too long: %X (%lX...%lx)", length, offset, offset + length);
    return ZFL_ERR_FAILURE;
  }

  buffer_index = 0;

  printf("\rData dest: %lX...%lx", offset, offset + length);

  fread(buffer_ch, 2, length + 1, file);

  pCh     = buffer_ch;
  pBuffer = buffer;

  while (buffer_index < length) {
    data = (*pBuffer = parse_byte(pCh));
    pBuffer++;
    buffer_index++;
    pCh += 2;
    check_sum += data;
  }

  data = parse_byte(pCh);
  check_sum += data;
  if (check_sum) {
    printf("\r\n<check_sum error> %X - %X", check_sum, data);
    return ZFL_ERR_VERIFY;
  }

  status = emit(offset, buffer, length);
  if (status != ZFL_ERR_SUCCESS) {
    printf("\r\nemit error: %X", status);
    return status;
  }

  return status;
}

int8_t process_hex_records(FILE *file, emit_func_t emit) {
  static int8_t  status = ZFL_ERR_SUCCESS;
  static uint8_t record_type;

  while (1) {
    record_type = process_hex_record_header(file);

    if (feof(file)) {
      printf("\r\nUnexpected EOF");
      return ZFL_ERR_FAILURE;
    }

    switch (record_type) {
    case (REC_TYPE_DATA): {
      status = process_hex_record_data(file, emit);
      if (status)
        return status;

      break;
    }

    case (REC_TYPE_EOF): {
      return process_hex_record_eof(file);
    }

    case (REC_TYPE_EXT_SEG_ADDR): {
      process_hex_record_ext_seg_addr(file);
      break;
    }

    case (REC_TYPE_START_SEG_ADDR): {
      process_hex_record_start_seg_addr(file);
      break;
    }

    case (REC_TYPE_EXT_LINEAR_ADDR): {
      status = process_hex_record_ext_linear_addr(file);
      if (status)
        return status;
      break;
    }

    case (REC_TYPE_START_LINEAR_ADDR): {
      process_hex_record_start_linear_addr(file);
      break;
    }

    default: {
      printf("\r\nUnknown Record record_type: %X", record_type);
      return ZFL_ERR_FAILURE;
    }
    }
  }
}
