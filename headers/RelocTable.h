#ifndef _RELOC_TABLE_H
#define _RELOC_TABLE_H

#include <stdint.h>


typedef enum RelocType {
	RELOC_TYPE_ABS = 0,    // Data to fix is in an immediate instruction
	RELOC_TYPE_MEM = 1,    // Data to fix is in a memory instruction
	RELOC_TYPE_IR24 = 2,
	RELOC_TYPE_IR19 = 3,
	RELOC_TYPE_DECOMP = 4,
	RELOC_TYPE_BYTE = 5,
	RELOC_TYPE_HWORD = 6,
	RELOC_TYPE_WORD = 7
} reloc_type_t;


typedef struct RelocEntry {
	uint32_t offset; // Offset within the section where relocation is to be applied
	uint32_t symbolIdx; // Index of the symbol in the symbol table
	reloc_type_t type; // Type of relocation
	int32_t addend; // Addend to be added to the symbol value
} RelocEnt;

typedef struct RelocTable {
	struct {
		RelocEnt** entries;
		uint32_t entryCount;
		uint32_t entryCapacity;
	} textRelocTable;

	struct {
		RelocEnt** entries;
		uint32_t entryCount;
		uint32_t entryCapacity;
	} dataRelocTable;

	struct {
		RelocEnt** entries;
		uint32_t entryCount;
		uint32_t entryCapacity;
	} constRelocTable;
} RelocTable;

// For use when passing relocation data to getting immediate encoding from encoding functions
// In order to avoid passing multiple arguments
typedef struct RelocationData {
	uint32_t lp;
	int32_t addend;
	reloc_type_t type;
	RelocTable* relocTable;
} RelData;


RelocTable* initRelocTable();
void deinitRelocTable(RelocTable* relocTable);

RelocEnt* initRelocEntry(uint32_t offset, uint32_t symbolIdx, reloc_type_t type, int32_t addend);

void addRelocEntry(RelocTable* relocTable, uint8_t section, RelocEnt* entry);

void displayRelocTable(RelocTable* relocTable);


#endif