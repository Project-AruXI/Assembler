.text

main:
	cmp x0, #10
	cmp x1, x2

	add x0, x0, #10
	add x0, x0, x1

	mv x10, #20
	mv x11, x10
	nop

	smul x2, x2, x3
	div x4, x5, x6

	ub main
	call unreachable
	ubr x10
	ret
	beq e

	SYSCALL
	HLT
	SI
	DI
	ERET
	LDIR x0
	MVCSTR x0
	LDCSTR x0
	RESR x0