#ifndef _RESERVED_KEYWORDS_H_
#define _RESERVED_KEYWORDS_H_

#include <strings.h>


static char* DIRECTIVES[] = {
	"data", "const", "bss", "text", "evt", "ivt", "set", "glob", "end",
	"string", "byte", "hword", "word", "float", "zero", "fill", "align",
	"size", "extern", "type", "sizeof", "def", "include", "typeinfo", "offset"
};

// These constants are used for indexing into/out of the DIRECTIVES array
// It is not to be used for token types

enum Directives {
	DATA, CONST, BSS, TEXT, EVT, IVT, SET, GLOB, END,
	STRING, BYTE, HWORD, WORD, FLOAT, ZERO, FILL, ALIGN,
	SIZE, EXTERN, TYPE, SIZEOF, DEF, INCLUDE, TYPEINFO, OFFSET
};

// The array is to be ordered by instruction types!!!
// This is vital to instruction parsing
static char* INSTRUCTIONS[] = {
	// I/R-Type (for instruction that have two types)
	"add", "adds", "sub", "subs",
	"or", "and", "xor", "not",
	"lsl", "lsr", "asr",
	"cmp", "mv", "mvn", // aliased

	// I-Type
	"nop", // aliased to add (imm)


	// R-Type
	"mul", "smul", "div", "sdiv",

	// M-Type
	"ld", "ldb", "ldbs", "ldbz", "ldh", "ldhs", "ldhz",
	"str", "strb", "strh",

	// Bi-Type
	"ub", "call",

	// Bu-Type
	"ubr", "ret",

	// Bc-Type
	"b",

	// S-Type
	"syscall", "hlt", "si", "di", "eret", "ldir",
	"mvcstr", "ldcstr", "resr"

	// F-Type

};

#define IR_TYPE_IDX 0
#define I_TYPE_IDX 14
#define R_TYPE_IDX 15
#define M_TYPE_IDX 19
#define Bi_TYPE_IDX 29
#define Bu_TYPE_IDX 31
#define Bc_TYPE_IDX 33
#define S_TYPE_IDX 34
#define F_TYPE_IDX 43

enum Instructions {
	ADD, ADDS, SUB, SUBS,
	OR, AND, XOR, NOT,
	LSL, LSR, ASR,
	CMP, MV, MVN,
	NOP,
	MUL, SMUL, DIV, SDIV,
	LD, LDB, LDBS, LDBZ, LDH, LDHS, LDHZ,
	STR, STRB, STRH,
	UB, CALL,
	UBR, RET,
	B,
	SYSCALL, HLT, SI, DI, ERET, LDIR, MVCSTR, LDCSTR, RESR
};

static char* REGISTERS[] = {
	"x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10",
	"x11", "x12", "x13", "x14", "x15", "x16", "x17", "x18", "x19", "x20",
	"x21", "x22", "x23", "x24", "x25", "x26", "x27", "x28", "x29", "x30", "x31", "sp",
	"xr", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", "a8", "a9",
	"c0", "c1", "c2", "c3", "c4", "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10",
	"lr", "xb", "xz", "ir"
};

enum Registers {
	X0, X1, X2, X3, X4, X5, X6, X7, X8, X9, X10,
	X11, X12, X13, X14, X15, X16, X17, X18, X19, X20,
	X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, SP,
	XR, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9,
	C0, C1, C2, C3, C4, S0, S1, S2, S3, S4, S5, S6, S7, S8, S9, S10,
	LR, XB, XZ, IR
};

// static char* CONDS[] = {
// 	"eq",
// 	"ne",
// 	"ov",
// 	"nv",
// 	"mi",
// 	"pz",
// 	"cc",
// 	"cs",
// 	"gt",
// 	"ge",
// 	"lt",
// 	"le"
// };


static inline int indexOf(char* arr[], int size, const char* key);
static inline int indexOf(char* arr[], int size, const char* key) {
	for (int i = 0; i < size; i++) {
		if (strcasecmp(arr[i], key) == 0) return i;
	}

	return -1;
}




#endif