#include <stdlib.h>

#include "RelocTable.h"
#include "diagnostics.h"


RelocTable* initRelocTable() {
	RelocTable* relocTable = (RelocTable*) malloc(sizeof(RelocTable));
	if (!relocTable) emitError(ERR_MEM, NULL, "Failed to allocate memory for relocation table.");

	relocTable->textRelocTable.entries = (RelocEnt**) malloc(sizeof(RelocEnt*) * 4);
	if (!relocTable->textRelocTable.entries) emitError(ERR_MEM, NULL, "Failed to allocate memory for text relocation entries.");
	relocTable->textRelocTable.entryCount = 0;
	relocTable->textRelocTable.entryCapacity = 4;

	relocTable->dataRelocTable.entries = (RelocEnt**) malloc(sizeof(RelocEnt*) * 4);
	if (!relocTable->dataRelocTable.entries) emitError(ERR_MEM, NULL, "Failed to allocate memory for data relocation entries.");
	relocTable->dataRelocTable.entryCount = 0;
	relocTable->dataRelocTable.entryCapacity = 4;

	relocTable->constRelocTable.entries = (RelocEnt**) malloc(sizeof(RelocEnt*) * 4);
	if (!relocTable->constRelocTable.entries) emitError(ERR_MEM, NULL, "Failed to allocate memory for const relocation entries.");
	relocTable->constRelocTable.entryCount = 0;
	relocTable->constRelocTable.entryCapacity = 4;

	return relocTable;
}
void deinitRelocTable(RelocTable* relocTable) {

}

RelocEnt* initRelocEntry(uint32_t offset, uint32_t symbolIdx, reloc_type_t type, int32_t addend) {
	RelocEnt* entry = (RelocEnt*) malloc(sizeof(RelocEnt));
	if (!entry) emitError(ERR_MEM, NULL, "Failed to allocate memory for relocation entry.");

	entry->offset = offset;
	entry->symbolIdx = symbolIdx;
	entry->type = type;
	entry->addend = addend;

	return entry;
}

void addRelocEntry(RelocTable* relocTable, uint8_t section, RelocEnt* entry) {
	RelocEnt*** entries = NULL;
	uint32_t* entryCount = NULL;
	uint32_t* entryCapacity = NULL;

	switch (section) {
		case 0: // Data section
			entries = &relocTable->dataRelocTable.entries;
			entryCount = &relocTable->dataRelocTable.entryCount;
			entryCapacity = &relocTable->dataRelocTable.entryCapacity;
			break;
		case 1: // Const section
			entries = &relocTable->constRelocTable.entries;
			entryCount = &relocTable->constRelocTable.entryCount;
			entryCapacity = &relocTable->constRelocTable.entryCapacity;
			break;
		case 3: // Text section
			entries = &relocTable->textRelocTable.entries;
			entryCount = &relocTable->textRelocTable.entryCount;
			entryCapacity = &relocTable->textRelocTable.entryCapacity;
			break;
		default:
			emitError(ERR_INTERNAL, NULL, "Invalid section %d for relocation entry.", section);
			break;
	}

	if (*entryCount == *entryCapacity) {
		*entryCapacity = (*entryCapacity == 0) ? 4 : (*entryCapacity * 2);
		RelocEnt** temp = (RelocEnt**) realloc(*entries, sizeof(RelocEnt*) * (*entryCapacity));
		if (!temp) emitError(ERR_MEM, NULL, "Failed to reallocate memory for relocation entries.");
		*entries = temp;
	}
	(*entries)[*entryCount] = entry;
	(*entryCount)++;
	log("Added relocation entry at offset 0x%08x in section %d", entry->offset, section);
}


static char* relocTypeToString(reloc_type_t type) {
	switch (type) {
		case RELOC_TYPE_ABS: return "ABS";
		case RELOC_TYPE_MEM: return "MEM";
		case RELOC_TYPE_IR24: return "IR24";
		case RELOC_TYPE_IR19: return "IR19";
		case RELOC_TYPE_DECOMP: return "DECOMP";
		case RELOC_TYPE_BYTE: return "BYTE";
		case RELOC_TYPE_HWORD: return "HWORD";
		case RELOC_TYPE_WORD: return "WORD";
		default: return "UNKNOWN";
	}
}

void displayRelocTable(RelocTable* relocTable) {
	const char* sectionNames[] = {"DATA", "CONST", "TEXT"};
	RelocEnt** entriesArr[] = {
		relocTable->dataRelocTable.entries,
		relocTable->constRelocTable.entries,
		relocTable->textRelocTable.entries
	};
	uint32_t sizes[] = {
		relocTable->dataRelocTable.entryCount,
		relocTable->constRelocTable.entryCount,
		relocTable->textRelocTable.entryCount
	};

	for (int s = 0; s < 3; ++s) {
		rtrace("\n================== %-5s Relocation Section ==================", sectionNames[s]);
		rtrace("Total Entries: %u", sizes[s]);
		rtrace("--------------------------------------------------------------------");
		rtrace("| %-4s | %-12s | %-10s | %-8s | %-8s |", "#", "Offset", "SymbolIdx", "Type", "Addend");
		rtrace("--------------------------------------------------------------------");
		for (uint32_t i = 0; i < sizes[s]; ++i) {
			RelocEnt* entry = entriesArr[s][i];
			rtrace("| %-4u | 0x%08x  | %-10u | %-8s | %-8d |",
				i, entry->offset, entry->symbolIdx, relocTypeToString(entry->type), entry->addend);
		}
		rtrace("--------------------------------------------------------------------\n");

		for (uint32_t i = 0; i < sizes[s]; ++i) {
			RelocEnt* e = entriesArr[s][i];
			rtrace("Reloc #%u -> Offset: 0x%08x, SymbolIdx: %u, Type: %s, Addend: %d",
				i, e->offset, e->symbolIdx, relocTypeToString(e->type), e->addend);
		}
	}
}