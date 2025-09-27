#ifndef _SECTION_TABLE_H_
#define _SECTION_TABLE_H_

#include <stdint.h>
#include <stdbool.h>


typedef enum {
	DATA_SECT_N,
	CONST_SECT_N,
	BSS_SECT_N,
	TEXT_SECT_N,
	EVT_SECT_N,
	IVT_SECT_N
} sect_table_n;

typedef struct SectEntry {
	uint32_t lp; // Location pointer
	uint32_t size; // Size of section (in bytes)
} section_entry_t;

typedef struct SectionTable {
	section_entry_t entries[6]; // 0 for data, 1 for const, 2 for bss, 3 for text, 4 for evt, 5 for ivt
	uint8_t activeSection; // 0 for data, 1 for const, 2 for bss, 3 for text, 4 for evt, 5 for ivt
} SectionTable;



SectionTable* initSectionTable();
void deinitSectionTable(SectionTable* sectTable);

void displaySectionTable(SectionTable* sectTable);


#endif