.extern ABSVAL
.extern ADDRVAL

.glob _init

.set LOCABS, #0xffff

.text
	_init:
		ld x0, =LOCABS % move big imm to x0
		ld x0, =LOCADDR % move addr to x0, emit rel
		ld x0, LOCADDR % move addr to x0, loads from it, emit rel

		ld x1, =ABSVAL % move 0 to x1, emit rel, linker is to ignore rel
		ld x1, =ADDRVAL % move 0 to x1, emit rel
		ld x1, ADDRVAL % move 0 to x1, loads from it, emit rel

.data
	LOCADDR: .word 0xae