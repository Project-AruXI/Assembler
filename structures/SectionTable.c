#include <stdio.h>
#include <stdlib.h>

#include "SectionTable.h"
#include "diagnostics.h"


SectionTable* initSectionTable() {
	SectionTable* sectTable = (SectionTable*) malloc(sizeof(SectionTable));
	// if (!sectTable) emitError(ERR_MEM, NULL, "Could not allocate memory for section table!\n");

	for (int i = 0; i < IVT_SECT_N+1; i++) {
		sectTable->entries[i].lp = 0x00000000;
		sectTable->entries[i].size = 0x00000000;
	}
	sectTable->activeSection = 0;

	return sectTable;
}

void displaySectionTable(SectionTable* sectTable) {
	const char* activeSectionStr = NULL;
	switch (sectTable->activeSection) {
		case DATA_SECT_N: activeSectionStr = "Data";  break;
		case CONST_SECT_N: activeSectionStr = "Const"; break;
		case BSS_SECT_N: activeSectionStr = "Bss";   break;
		case TEXT_SECT_N: activeSectionStr = "Text";  break;
		case EVT_SECT_N: activeSectionStr = "EVT";   break;
		case IVT_SECT_N: activeSectionStr = "IVT";   break;
		default: activeSectionStr = "Unknown"; break;
	}

	rtrace("\n============= Section Table =============");
	rtrace("Active Section: %s (%d)", activeSectionStr, sectTable->activeSection);
	rtrace("-----------------------------------------");
	rtrace("| %-7s | %-12s | %-10s |", "Section", "Location Ptr", "Size (bytes)");
	rtrace("-----------------------------------------");
	rtrace("| %-7s | 0x%08x   | %-12u |", "Data",  sectTable->entries[DATA_SECT_N].lp,  sectTable->entries[DATA_SECT_N].size);
	rtrace("| %-7s | 0x%08x   | %-12u |", "Const", sectTable->entries[CONST_SECT_N].lp, sectTable->entries[CONST_SECT_N].size);
	rtrace("| %-7s | 0x%08x   | %-12u |", "Bss",   sectTable->entries[BSS_SECT_N].lp,   sectTable->entries[BSS_SECT_N].size);
	rtrace("| %-7s | 0x%08x   | %-12u |", "Text",  sectTable->entries[TEXT_SECT_N].lp,  sectTable->entries[TEXT_SECT_N].size);
	rtrace("| %-7s | 0x%08x   | %-12u |", "EVT",   sectTable->entries[EVT_SECT_N].lp,   sectTable->entries[EVT_SECT_N].size);
	rtrace("| %-7s | 0x%08x   | %-12u |", "IVT",   sectTable->entries[IVT_SECT_N].lp,   sectTable->entries[IVT_SECT_N].size);
	rtrace("-----------------------------------------\n");
}

void deinitSectionTable(SectionTable* sectTable) {
	free(sectTable);
}