.text
	% ADD, ADDS, SUB, SUBS,
	% OR, AND, XOR, NOT,
	% LSL, LSR, ASR,
	% CMP, MV, MVN,
	% NOP,

	add x0, x1, #0x3fff
	add x0, x1, #0x2000 % should error, out of range
	add x0, x1, #0x1fff
	add x0, x1, #0x0
	add x0, x1, #0x1 % smallest positive
	add x0, x1, #-0x1 % smallest negative
	add x0, x1, #-0x2000 % should error, out of range
	add x0, x1, #-0x1fff
	sub x0, x1, #0x3fff
	sub x0, x1, #0x2000 % should error, out of range
	sub x0, x1, #0x1fff
	sub x0, x1, #0x0