#include <stdio.h>
#include <stdlib.h>

#include "SectionTable.h"


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
	// debug(DEBUG_TRACE, "Section Table (active section: %d):\n", sectTable->activeSection);

	// debug(DEBUG_TRACE, "\tData Section (0):\n");
	// debug(DEBUG_TRACE, "\t\tLocation Pointer: 0x%x\n", sectTable->entries[DATA].lp);
	// debug(DEBUG_TRACE, "\t\tSize: 0x%x bytes\n", sectTable->entries[DATA].size);
	// debug(DEBUG_TRACE, "\t\tIs present: %s\n", (sectTable->entries[DATA].present ? "true" : "false"));

	// debug(DEBUG_TRACE, "\tConst Section (1):\n");
	// debug(DEBUG_TRACE, "\t\tLocation Pointer: 0x%x\n", sectTable->entries[CONST].lp);
	// debug(DEBUG_TRACE, "\t\tSize: 0x%x bytes\n", sectTable->entries[CONST].size);
	// debug(DEBUG_TRACE, "\t\tIs present: %s\n", (sectTable->entries[CONST].present ? "true" : "false"));

	// debug(DEBUG_TRACE, "\tBss Section (2):\n");
	// debug(DEBUG_TRACE, "\t\tLocation Pointer: 0x%x\n", sectTable->entries[BSS].lp);
	// debug(DEBUG_TRACE, "\t\tSize: 0x%x bytes\n", sectTable->entries[BSS].size);
	// debug(DEBUG_TRACE, "\t\tIs present: %s\n", (sectTable->entries[BSS].present ? "true" : "false"));

	// debug(DEBUG_TRACE, "\tText Section (3):\n");
	// debug(DEBUG_TRACE, "\t\tLocation Pointer: 0x%x\n", sectTable->entries[TEXT].lp);
	// debug(DEBUG_TRACE, "\t\tSize: 0x%x bytes\n", sectTable->entries[TEXT].size);
	// debug(DEBUG_TRACE, "\t\tIs present: %s\n", (sectTable->entries[TEXT].present ? "true" : "false"));

	// debug(DEBUG_TRACE, "\tEVT Section (4):\n");
	// debug(DEBUG_TRACE, "\t\tLocation Pointer: 0x%x\n", sectTable->entries[EVT].lp);
	// debug(DEBUG_TRACE, "\t\tSize: 0x%x bytes\n", sectTable->entries[EVT].size);
	// debug(DEBUG_TRACE, "\t\tIs present: %s\n", (sectTable->entries[EVT].present ? "true" : "fa;se"));
}

void deinitSectionTable(SectionTable* sectTable) {
	free(sectTable);
}