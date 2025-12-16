.set EXPR0, #10 % when single number, must use `#`
.set EX, 20 + EXR
.set EXPR1, EXPR0 + 20 % `#` is optional in expressions
.set EXPR2, EXPR1 * 2 - 5 / (3 + 1) / ((-2 + 3) * 4) + 7 - 8 * 0x2 / 4 + 6
% expression using bitwise operators
.set EXPR3, EXPR2 & 0xff | (0x1 << 4) | (0x2 << 8) | (0x3 << 12) | (0x4 << 16) | (0x5 << 20) | (0x6 << 24) | (0x7 << 28)
% using xor and not
.set EXPR4, EXPR3 ^ ~0xffffffff ^ 0xaaaaaaaa ^ 0x55555555

.text
  %mv x0, #0xa
	%mv x0, EXPR0
	%mv x0, #10 + 20
	%mv x0, 10 + EXPR1
	mv x0, EXPR2 + 15 - 3 * 2 / (1 + 1)
	%mv x0, (EXPR2 + 15 - 3) * 2 / (1 + 1)
  .end

loads:
  ld x1, =EXPR0
  ld x1, EXPR0
  % .size loads, @- loads

  ld x1, [x2, #0x4]
  ld x1, [x2, EXPR0]
  ld x1, [x2, EXPR1 + #4]
  ld x1, [x2, EXPR2 - 8 / #0x2]
  ld x1, [x2, (EXPR2 - 0xa) / 2]

%.def Node {
%  next:32.
%  value:8.
%}

%.type OBJ, $object.struct.Node
%.type OBJ_PTR, $object.ptr.Node

type_loads:
  %ld x2, OBJ.next % load field 'next', which is represented to be an address, so it technically is doing ld x2, 0xff000000
  %ld x3, OBJ_PTR.next % same thing as above but through a pointer, also doing ld x3, 0xff000000

.data
  OBJ: 
    .hword 0xff000000
    .byte 0x7f

  OBJ_PTR: .hword OBJ