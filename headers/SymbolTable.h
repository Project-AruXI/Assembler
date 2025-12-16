#ifndef _SYMBOL_TABLE_H_
#define _SYMBOL_TABLE_H_

#include <stdint.h>

#include "ast.h"
#include "libsecuredstring.h"

typedef uint32_t SYMBFLAGS;

typedef struct SymbolEntryReference {
	SString* source; // The source line where the symbol is referenced
	int linenum; // The line where the symbol is referenced
} symb_entry_ref_t;

typedef struct SymbolEntry {
	char* name;
	SYMBFLAGS flags;
	uint32_t size; // The size of the symbol in bytes, it can either be explicitly set (via .size) or inferred

	SString* source; // The source line where the symbol is defined
	int linenum; // The line where the symbol is defined

	union {
		Node* expr; // If the symbol is defined as an expression, the root of the expression AST
		uint32_t val; // If the symbol is defined as a constant value, either as an absolute address or via a resolved expression
	} value;

	struct References {
		symb_entry_ref_t** refs;
		int refcount;
		int refcap;
	} references;

	// In the case that the symbol is typed to be a specific struct/union. -1 for none
	int structTypeIdx;

	int symbTableIndex;
} symb_entry_t;

typedef struct SymbolTable {
	symb_entry_t** entries; // Symbol entries
	uint32_t size; // Number of entries
	uint32_t capacity;
} SymbolTable;


/**
	| 15-12 | 11 | 10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
	| x     |  M |  M | T | T | T | E | S | S | S | L | R | D |
	M: The main type of symbol. `00` for none (extern or unknown), `01` for absolute, `10` for function, `11` for object
	T: The subtype of symbol, depending on M. `00` for none, `01` for array, `10` for struct, `11` for union, `100` for pointer
	E: Whether the symbol hold an expression AST or a value. `0` for value, `1` for expression
	S: The section the symbol is defined in. `00` for data, `01` for const, `10` for  bss, `11` for text, `100` for evt, `101` for ivt, `111` for undefined.
	L: The locality of the symbol. `0` for local, and `1` for global.
	R: The reference state of the symbol. `0` if not referenced, and `1` if referenced.
	D: The defined state of the symbol. `0` for undefined, and `1` for defined.
*/

#define M_NONE 0b00 // Extern or Unknown
#define M_ABS 0b01 // Absolute number
#define M_FUNC 0b10 // Function
#define M_OBJ 0b11 // Object
#define T_NONE 0b00 // No subtype
#define T_ARR  0b01 // Array
#define T_STRUCT 0b10 // Struct
#define T_UNION 0b11 // Union
#define T_PTR  0b100 // Pointer
#define S_DATA 0b000 // data section
#define S_CONST 0b001 // const section
#define S_BSS 0b010 // bss section
#define S_TEXT 0b011 // text section
#define S_EVT 0b100 // evt section
#define S_IVT 0b101 // ivt section
#define S_UNDEF 0b111 // Undefined section
#define E_EXPR 1
#define E_VAL 0
#define L_LOC 0
#define L_GLOB 1
#define R_NREF 0
#define R_REF 1
#define D_UNDEF 0
#define D_DEF 1


#define CREATE_FLAGS(m, t, e, s, l, r, d) ((m << 10) | (t << 7) | (e << 6) | (s << 3) | (l << 2) | (r << 1) | (d << 0))

#define GET_MAIN_TYPE(flags) ((flags >> 10) & 0b11)
#define GET_SUB_TYPE(flags) ((flags >> 7) & 0b111)
#define GET_EXPRESSION(flags) ((flags >> 6) & 0b1)
#define GET_SECTION(flags) ((flags >> 3) & 0b111)
#define GET_LOCALITY(flags) ((flags >> 2) & 0b1)
#define GET_REFERENCED(flags) ((flags >> 1) & 0b1)
#define GET_DEFINED(flags) ((flags >> 0) & 0b1)

#define SET_MAIN_TYPE(flags, type) ((flags & ~(0b11 << 10)) | ((type & 0b11) << 10)) // Sets the type to the given type
#define SET_SUB_TYPE(flags, type) ((flags & ~(0b111 << 7)) | ((type & 0b111) << 7)) // Sets the subtype to the given type
#define SET_EXPRESSION(flags) (flags |= (1 << 6)) // Sets the expression flag to 1
#define SET_SECTION(flags, section) ((flags & ~(0b111 << 3)) | ((section & 0b111) << 3)) // Sets the section to the given section
#define SET_LOCALITY(flags) (flags |= (1 << 2)) // Sets the locality flag to 1 for global
#define SET_REFERENCED(flags) (flags |= (1 << 1)) // Sets referenced
#define SET_DEFINED(flags) (flags |= (1 << 0)) // Sets defined

#define CLR_EXPRESSION(flags) (flags &= ~(1 << 6)) // Sets the expression flag to 0


SymbolTable* initSymbolTable();
void deinitSymbolTable(SymbolTable* table);

symb_entry_t* initSymbolEntry(const char* name, SYMBFLAGS flags, Node* expr, uint32_t val, SString* source, int linenum);
void deinitSymbolEntry(symb_entry_t* entry);

void addSymbolEntry(SymbolTable* table, symb_entry_t* entry);
void addSymbolReference(symb_entry_t* entry, SString* source, int linenum);

symb_entry_t* getSymbolEntry(SymbolTable* table, const char* name);

void updateSymbolEntry(symb_entry_t* entry, SYMBFLAGS flags, uint32_t value);

void displaySymbolTable(SymbolTable* table);
void displaySymbolEntry(symb_entry_t* entry);

#endif