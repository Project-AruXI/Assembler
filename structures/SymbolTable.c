#include <stdlib.h>
#include <string.h>

#include "SymbolTable.h"
#include "diagnostics.h"


SymbolTable* initSymbolTable() {
	SymbolTable* symbTable = (SymbolTable*) malloc(sizeof(SymbolTable));
	if (!symbTable) emitError(ERR_MEM, NULL, "Failed to allocate memory for symbol table.");

	symbTable->entries = (symb_entry_t**) malloc(sizeof(symb_entry_t*) * 10);
	if (!symbTable->entries) emitError(ERR_MEM, NULL, "Failed to allocate memory for symbol table entries.");
	symbTable->size = 0;
	symbTable->capacity = 10;

	return symbTable;
}

void deinitSymbolTable(SymbolTable* table) {
	for (uint32_t i = 0; i < table->size; ++i) {
		deinitSymbolEntry(table->entries[i]);
	}
	free(table->entries);
	free(table);
}

symb_entry_t* initSymbolEntry(const char* name, SYMBFLAGS flags, Node* expr, uint32_t val, SString* source, int linenum) {
	symb_entry_t* entry = (symb_entry_t*)malloc(sizeof(symb_entry_t));
	if (!entry) emitError(ERR_MEM, NULL, "Failed to allocate memory for symbol entry.");

	entry->name = strdup(name);
	entry->flags = flags;
	entry->size = 0;
	entry->source = source; // Maybe have the entry use its own copy of SString
	entry->linenum = linenum;

	if (!expr) entry->value.val = val;
	else entry->value.expr = expr;

	entry->references.refs = (symb_entry_ref_t**) malloc(sizeof(symb_entry_ref_t*) * 4);
	if (!entry->references.refs) emitError(ERR_MEM, NULL, "Failed to allocate memory for symbol entry references.");
	entry->references.refcount = 0;
	entry->references.refcap = 4;

	entry->structTypeIdx = -1;

	entry->symbTableIndex = -1; // Will be set when added to the symbol table

	return entry;
}

void deinitSymbolEntry(symb_entry_t* entry) {
	// If entry has its own copy of SString, free it here; if not, it is managed in the Token struct
	// If the entry still contains the expression AST, that is managed by the parser

	for (int i = 0; i < entry->references.refcount; ++i) {
		if (entry->references.refs[i]) free(entry->references.refs[i]);
	}
	free(entry->references.refs);
	free(entry->name);
	free(entry);
}

void addSymbolEntry(SymbolTable* table, symb_entry_t* entry) {
	if (table->size == table->capacity) {
		table->capacity += 5;
		table->entries = (symb_entry_t**)realloc(table->entries, sizeof(symb_entry_t*) * table->capacity);
	}
	table->entries[table->size++] = entry;
	entry->symbTableIndex = table->size - 1;
}

void addSymbolReference(symb_entry_t* entry, SString* source, int linenum) {
	if (entry->references.refcount == entry->references.refcap) {
		entry->references.refcap += 5;
		symb_entry_ref_t** temp = (symb_entry_ref_t**) realloc(entry->references.refs, sizeof(symb_entry_ref_t*) * entry->references.refcap);
		if (!temp) emitError(ERR_MEM, NULL, "Failed to reallocate memory for symbol entry references.");
		entry->references.refs = temp;
	}

	symb_entry_ref_t* ref = (symb_entry_ref_t*) malloc(sizeof(symb_entry_ref_t));
	ref->source = source; // Maybe have the references keep its own copy of SString
	ref->linenum = linenum;
	entry->references.refs[entry->references.refcount++] = ref;
}

symb_entry_t* getSymbolEntry(SymbolTable* table, const char* name) {
	for (uint32_t i = 0; i < table->size; ++i) {
		if (strcmp(table->entries[i]->name, name) == 0) return table->entries[i];
	}

	return NULL;
}

void updateSymbolEntry(symb_entry_t* entry, SYMBFLAGS flags, uint32_t value) {
	entry->flags = flags;
	entry->value.val = value;
}

static char* flagToString(SYMBFLAGS flags) {
	static char buffer[64];
	buffer[0] = '\0';

	// Main type
	switch (GET_MAIN_TYPE(flags)) {
		case M_NONE: strcat(buffer, "M_NONE "); break;
		case M_ABS: strcat(buffer, "M_ABS "); break;
		case M_FUNC: strcat(buffer, "M_FUNC "); break;
		case M_OBJ: strcat(buffer, "M_OBJ "); break;
		default: strcat(buffer, "M_UNKNOWN "); break;
	}

	// Sub type
	switch (GET_SUB_TYPE(flags)) {
		case T_NONE: strcat(buffer, "T_NONE "); break;
		case T_ARR: strcat(buffer, "T_ARR "); break;
		case T_STRUCT: strcat(buffer, "T_STRUCT "); break;
		case T_UNION: strcat(buffer, "T_UNION "); break;
		case T_PTR: strcat(buffer, "T_PTR "); break;
		default: strcat(buffer, "T_UNKNOWN "); break;
	}

	// Expression or value
	if (GET_EXPRESSION(flags)) strcat(buffer, "E_EXPR ");
	else strcat(buffer, "E_VAL ");

	// Section
	switch (GET_SECTION(flags)) {
		case S_DATA: strcat(buffer, "S_DATA "); break;
		case S_CONST: strcat(buffer, "S_CONST "); break;
		case S_BSS: strcat(buffer, "S_BSS "); break;
		case S_TEXT: strcat(buffer, "S_TEXT "); break;
		case S_EVT: strcat(buffer, "S_EVT "); break;
		case S_IVT: strcat(buffer, "S_IVT "); break;
		case S_UNDEF: strcat(buffer, "S_UNDEF "); break;
		default: strcat(buffer, "S_UNKNOWN "); break;
	}

	// Locality
	if (GET_LOCALITY(flags)) strcat(buffer, "L_GLOB ");
	else strcat(buffer, "L_LOC ");

	// Referenced
	if (GET_REFERENCED(flags)) strcat(buffer, "R_REF ");
	else strcat(buffer, "R_NREF ");

	// Defined
	if (GET_DEFINED(flags)) strcat(buffer, "D_DEF");
	else strcat(buffer, "D_UNDEF");

	return buffer;
}

void displaySymbolTable(SymbolTable* table) {
		if (!table || table->size == 0) {
			rtrace("\n[Symbol Table is empty]\n");
			return;
		}
		rtrace("\n=================== Symbol Table ====================");
		rtrace("Total Symbols: %u (capacity: %u)", table->size, table->capacity);
		rtrace("-----------------------------------------------------------------------------------------------------------------");
		rtrace("| %-3s | %-20s | %-45s | %-12s | %-8s | %-6s |", "#", "Name", "Flags", "Size (bytes)", "Line", "Refs");
		rtrace("-----------------------------------------------------------------------------------------------------------------");
		for (uint32_t i = 0; i < table->size; ++i) {
			symb_entry_t* entry = table->entries[i];
			rtrace("| %-3u | %-20s | %-45s | %-12u | %-8d | %-6d |",
				i, entry->name, flagToString(entry->flags), entry->size, entry->linenum, entry->references.refcount);
		}
		rtrace("-----------------------------------------------------------------------------------------------------------------\n");

		for (uint32_t i = 0; i < table->size; ++i) {
			displaySymbolEntry(table->entries[i]);
		}
}


void displaySymbolEntry(symb_entry_t* entry) {
		rtrace("\n------------------- Symbol Entry -------------------");
		rtrace("Name:   %s", entry->name);
		rtrace("Flags:  %s", flagToString(entry->flags));
		rtrace("Size:   %u bytes", entry->size);
		rtrace("Line:   %d", entry->linenum);
		rtrace("Source: %s", (entry->source) ? ssGetString(entry->source) : "(unknown)");
		if (GET_EXPRESSION(entry->flags)) {
			// Maybe have an option to print the AST in a nice format
			rtrace("Value:  [Expression AST]");
		} else {
			rtrace("Value:  0x%x", entry->value.val);
		}
		rtrace("References (%d):", entry->references.refcount);
		if (entry->references.refcount > 0) {
			rtrace("  | %-3s | %-6s |", "#", "Line");
			for (int j = 0; j < entry->references.refcount; ++j) {
				symb_entry_ref_t* ref = entry->references.refs[j];
				rtrace("  | %-3d | %-6d |", j, ref->linenum);
			}
		}
		rtrace("----------------------------------------------------\n");
}