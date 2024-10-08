
        INCLUDE "startup\ez80f92.inc"

	SECTION CODE

	.assume adl=1

	public	_spike

_spike:
	ei

	; write spike code here to be run in ADL mode on  on-chip rom


	; copy code from rom to ram and run it in Z80 mode
	LD	HL, step1
	ld	de, 02E300h
	ld	bc, step1_size
	ldir

	LD	A, 02H
	LD	MB, A
	LD.SIS	SP, 0E700H
	NOP
	NOP
	LD	HL, %451234
	LD	E, %66
	CALL.IS	0E300H		; run the z80 code in RAM
	RET


	SEGMENT CODE

	.assume adl = 0
step1:

	; write code here to be run in z80 mode on on-chip rom

step1_end:

step1_size	equ	step1_end-step1
