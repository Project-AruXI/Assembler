.text
	fxn0:
		nop
		hlt

.evt
	ld x0, =ptr


	.byte 0b00000000
	.hword 0x0000
	.byte 0x0
	.word fxn0

	.byte 0b00000001
	.hword 0x0000
	.byte 0x0
	.word fxn1


.data
	ptr: .word 0x0

.text
	fxn1:
		nop
		hlt