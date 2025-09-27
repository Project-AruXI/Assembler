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

symb_entry_t* initSymbolEntry(const char* name, SYMBFLAGS flags, SString* source, int linenum) {
	symb_entry_t* entry = (symb_entry_t*)malloc(sizeof(symb_entry_t));
	if (!entry) emitError(ERR_MEM, NULL, "Failed to allocate memory for symbol entry.");

	entry->name = strdup(name);
	entry->flags = flags;
	entry->size = 0;
	entry->source = source; // Maybe have the entry use its own copy of SString
	entry->linenum = linenum;

	entry->value.expr = NULL;

	entry->references.refs = (symb_entry_ref_t**) malloc(sizeof(symb_entry_ref_t*) * 4);
	if (!entry->references.refs) emitError(ERR_MEM, NULL, "Failed to allocate memory for symbol entry references.");
	entry->references.refcount = 0;
	entry->references.refcap = 4;

	entry->structTypeIdx = -1;

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

// TODO: find a better layout
void displaySymbolTable(SymbolTable* table) {
	for (uint32_t i = 0; i < table->size; ++i) {
		symb_entry_t* entry = table->entries[i];
		log("Symbol: %s, Flags: 0x%X, Size: %u, Defined at line %d\n", entry->name, entry->flags, entry->size, entry->linenum);
		if (GET_EXPRESSION(entry->flags)) {
			log("  Value: [Expression AST]\n");
		} else {
			log("  Value: %u\n", entry->value.val);
		}
		log("  References (%d):\n", entry->references.refcount);
		for (int j = 0; j < entry->references.refcount; ++j) {
			symb_entry_ref_t* ref = entry->references.refs[j];
			log("    Line %d\n", ref->linenum);
		}
	}
}