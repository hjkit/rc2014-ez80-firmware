        INCLUDE "..\config.inc"

	SECTION CODE

	.assume adl=1

	PUBLIC	_init_clocks
	PUBLIC	_get_ticks_for_rtcclk
	PUBLIC	_configure_tmr1_to_rtc
	PUBLIC	_configure_tmr1_to_sysclk
	PUBLIC	_get_ticks_for_sysclk

	XREF	_system_ticks
	XREF	_ticks_frequency
	XREF	_cpu_freq_calculated
	XREF	_assign_cpu_frequency
	XREF	_rtc_enabled

	XREF	__lshru
	XREF	__idivu

RTC_CLOCK_RATE EQU 32768

COUNT_FOR_60HZ_RTC EQU RTC_CLOCK_RATE / 60

_init_clocks:
	IN0	A, (RTC_CTRL)
	AND	%70 ; 01110000
	CP	RTC_INT_DISABLE | RTC_BCD_ENABLE | RTC_CLK_32KHZ
	JR	Z, rtc_already_enabled

	; CONFIGURE RTC TO USE 32KHZ CLOCK AND RESET COUNTERS TO 0
	LD	A, RTC_INT_DISABLE | RTC_BCD_ENABLE | RTC_CLK_32KHZ | RTC_UNLOCK
	OUT0	(RTC_CTRL), A

	XOR	A
	OUT0	(RTC_CEN), A
	OUT0	(RTC_YR), A
	OUT0	(RTC_MON), A
	OUT0	(RTC_DOM), A
	OUT0	(RTC_HRS), A
	OUT0	(RTC_MIN), A
	OUT0	(RTC_SEC), A

	LD	A, RTC_INT_DISABLE | RTC_BCD_ENABLE | RTC_CLK_32KHZ | RTC_LOCK
	OUT0	(RTC_CTRL), A

rtc_already_enabled:
	LD	HL, 1
	CALL	_configure_tmr1_to_rtc			; CONFIGURE TM1 FOR RTC CLOCK AS FULL SPEED

	EI						; SEE IF THE TIMER ISR IS TRIGGERED
	LD	HL, _system_ticks			; BY NOTING IF _system_ticks CHANGES
	LD	A, (HL)
	LD	B, 0
wait:
	PUSH	AF
	PUSH	BC
	POP	BC
	POP	AF
	DJNZ	wait
	DI

	CP	(HL)
	JR	Z, no_rtc

	LD	HL, COUNT_FOR_60HZ_RTC
	CALL	_configure_tmr1_to_rtc

	CALL	measure_cpu_freq			; MEASURE CPU FREQUENCY WITH A 60HZ CLOCK

	CALL	_get_ticks_for_rtcclk
	CALL	_configure_tmr1_to_rtc

	LD	HL, 0
	LD	(_system_ticks), HL

	CPL						; RETURN NZ FOR RTC PRESENT
	LD	(_rtc_enabled), A
	RET

no_rtc:
	CALL	_get_ticks_for_sysclk
	CALL	_configure_tmr1_to_sysclk

	XOR	A					; RETURN 0 FOR NO RTC
	LD	(_rtc_enabled), A
	RET

_configure_tmr1_to_rtc:
	LD	A, TMR0_IN_SYSCLK | TMR1_IN_RTC | TMR2_IN_SYSCLK | TMR3_IN_SYSCLK
	OUT0	(TMR_ISS), A

configure_tmr1:
	LD	A, L
	OUT0	(TMR1_RR_L), A
	LD	A, H
	OUT0	(TMR1_RR_H), A
	LD	A, TMR_ENABLED | TMR_CONTINUOUS | TMR_RST_EN | TMR_CLK_DIV_16 | TMR_IRQ_EN  ; CLK DIV NOT APPLIED IF BASED ON RTC CLOCK SOURCE
	OUT0	(TMR1_CTL), A

	RET

_configure_tmr1_to_sysclk:
	LD	A, TMR0_IN_SYSCLK | TMR1_IN_SYSCLK | TMR2_IN_SYSCLK | TMR3_IN_SYSCLK
	OUT0	(TMR_ISS), A

	JR	configure_tmr1


measure_cpu_freq:
	EI
	; // use timer ticks (60Hz) to evaluate the current CPU frequency
repeat:
	LD	HL, _system_ticks

	LD	DE, 0
	LD	C, 7

	LD	A, (HL)
sync_tick:
	CP	(HL)					; SYNC TO NEXT TICK EDGE
	JR	Z, sync_tick

	ADD	C

cycle_counter:
	INC	DE
	CP	(HL)					; COUNT FOR 6 TICKS
	JR	NZ, cycle_counter

skip:
	PUSH	DE
	CALL	_assign_cpu_frequency
	POP	DE

	DI
	RET
;
; Determine the timer counter to generate a 50Hz or 60Hz timer based on the cpu clock
; Output
;   HL = (_cpu_freq_calculated/16)/_ticks_frequency
; ASSUMES A FREQUENCY NO MORE THAN 28 BITS
;
_get_ticks_for_sysclk:
	LD	BC, (_cpu_freq_calculated)		; DIVIDE CPU FREQUENCY
	LD	A, (_cpu_freq_calculated+3)		; BY 16
	LD	L, 4
	CALL	__lshru
	LD	HL, BC					; uHL = _cpu_freq_calculated/16

	LD	BC, 0
	LD	A, (_ticks_frequency)
	LD	C, A					; uBC = ticks_frequency

	JP	__idivu					; uHL = uHL/uBC
;
; Determine the timer counter to generate a 50Hz or 60Hz timer based on the RTC
; Output
;  HL = RTC_CLOCK_RATE/_ticks_frequency
;
_get_ticks_for_rtcclk:
	LD	BC, 0
	LD	A, (_ticks_frequency)
	LD	C, A					; AuBC = ticks_frequency
	LD	HL, RTC_CLOCK_RATE
	JP	__idivu
