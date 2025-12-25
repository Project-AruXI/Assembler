.extern FXN
.extern EXTVAL
.extern BIGVAL
.extern ARR

.text
_start:
    % --- IR19: conditional branch ---
    beq FXN

    % --- IR24: unconditional branch ---
    ub   FXN

    % --- ABS14: small absolute immediate ---
    % SMALLVAL is local and fits in 14 bits
    add x1, x0, SMALLVAL

    % --- DECOMP: large immediate load ---
    ld   x3, =ARR


.data
    % --- ABS8: 8‑bit data relocation ---
    .byte SMALLVAL

    % --- ABS16: 16‑bit data relocation ---
    .hword SMALLVAL, BIGVAL

    % --- ABS32: 32‑bit data relocation ---
    .word SMALLVAL

SMALLVAL:
    .word 42


.const
    % Another ABS32 test in a different section
    .word EXTVAL