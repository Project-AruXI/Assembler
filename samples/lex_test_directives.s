% Test directive lexing
.data
.include "./samples/lex_test_member_defs.adecl"
arr: .byte 1, 2, 3
.string "Hello, World!"
.float 3.14, 2.71
.set CONST_VAL, 42
.set ANOTHER_VAL, CONST_VAL + 8
.text
    ret

.type NODE_STRUCT, $object.struct.Node