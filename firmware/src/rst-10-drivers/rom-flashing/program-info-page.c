#pragma modsect FLASH_INFO_PAGE

#include "rom-flashing.h"
#include <critical-blocks.h>
#include <eZ80F92-extra.h>
#include <stdint.h>

int8_t _IFL_EraseInfoPage(const uint8_t addr) {
  uint8_t Page;
  int8_t  status = ZFL_ERR_SUCCESS;

  critical_begin();
  Page = FLASH_PAGE;

  FLASH_KEY  = FLASH_KEY_UNLOCK_1;
  FLASH_KEY  = FLASH_KEY_UNLOCK_2;
  FLASH_PROT = 0xFE;

  FLASH_PAGE = FLASH_PAGE_INFO_EN;
  FLASH_ROW  = addr & IFL_ROW_SIZE ? 1 : 0;

  FLASH_PGCTL = FLASH_PGCTL_PG_ERASE;
  while (FLASH_PGCTL & FLASH_PGCTL_PG_ERASE)
    ;
  if (FLASH_IRQ & FLASH_IRQ_PG_VIO)
    status = ZFL_ERR_ERASE;

  FLASH_PAGE = Page;

  FLASH_KEY  = FLASH_KEY_UNLOCK_1;
  FLASH_KEY  = FLASH_KEY_UNLOCK_2;
  FLASH_PROT = 0xFF;

  critical_end();

  return status;
}

int8_t _IFL_ProgramInfoPage(uint8_t hDst, uint8_t *hSrc, uint8_t Len) {
  uint16_t i;
  uint8_t  status = ZFL_ERR_SUCCESS;
  uint8_t  Page;
  uint8_t *pDst = (uint8_t *)((uint24_t)hDst);

  critical_begin();
  Page = FLASH_PAGE;

  FLASH_KEY  = FLASH_KEY_UNLOCK_1;
  FLASH_KEY  = FLASH_KEY_UNLOCK_2;
  FLASH_PROT = 0xFE;

  FLASH_PAGE = FLASH_PAGE_INFO_EN;

  for (i = 0; i < Len; i++) {
    if (hDst & IFL_ROW_SIZE)
      FLASH_ROW = 1;
    else
      FLASH_ROW = 0;

    FLASH_COL = hDst;

    FLASH_DATA = *hSrc;

    if (FLASH_IRQ & FLASH_IRQ_WR_VIO)
      status = ZFL_ERR_WRITE;

    hDst++;
    hSrc++;
  }

  FLASH_PAGE = Page;

  FLASH_KEY  = FLASH_KEY_UNLOCK_1;
  FLASH_KEY  = FLASH_KEY_UNLOCK_2;
  FLASH_PROT = 0xFF;

  critical_end();

  return status;
}

int8_t _IFL_ReadInfoPage(uint8_t *pDst, uint8_t _pSrc, uint8_t Len) {
  uint8_t  x;
  uint16_t i;
  uint8_t  Page;
  uint8_t *pSrc = (uint8_t *)((uint24_t)_pSrc);

  critical_begin();
  Page = FLASH_PAGE;

  FLASH_PAGE = FLASH_PAGE_INFO_EN;

  for (i = 0; i < Len; i++)
    *pDst++ = *pSrc++;

  FLASH_PAGE = Page;
  critical_end();

  return ZFL_ERR_SUCCESS;
}

int8_t _IFL_WriteInfoByte(uint16_t addr_and_data) {
  int8_t status;

  const uint8_t data = (uint8_t)(addr_and_data >> 8);
  const uint8_t addr = (uint8_t)(addr_and_data & 0xFF);

  uint8_t buffer[IFL_ROW_SIZE];

  status = _IFL_ReadInfoPage(buffer, addr & IFL_ROW_SIZE, IFL_ROW_SIZE);

  if (status != ZFL_ERR_SUCCESS)
    return status;

  buffer[addr & ~IFL_ROW_SIZE] = data;

  status = _IFL_EraseInfoPage(addr);

  if (status != ZFL_ERR_SUCCESS)
    return status;

  return _IFL_ProgramInfoPage(addr & IFL_ROW_SIZE, buffer, IFL_ROW_SIZE);
}
