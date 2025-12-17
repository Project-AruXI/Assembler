#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "codegen.h"
#include "diagnostics.h"
#include "aoef.h"




static uint32_t getSymbolStringsSize(SymbolTable* symbTable) {
	uint32_t totalSize = 16; // Table has ending string of size 16
	for (uint32_t i = 0; i < symbTable->size; i++) {
		symb_entry_t* entry = symbTable->entries[i];
		totalSize += strlen(entry->name) + 1; // +1 for null terminator
	}

	return totalSize;
}

/**
 * Like `strcat` but preserves the previous null byte instead.
 */
static char* nstrcat(char* dest, const char* src) {
	while (*src) {
		*dest = *src;
		src++;
		dest++;
	}

	// src now at null byte, copy it
	*dest = *src;
	dest++;
	// dest now after null

	return dest;
}

/**
 * Like `strncat` but preserves the previous null byte instead.
 * @param dest
 * @param src
 */
static char* nstrncat(char* dest, const char* src, size_t n) {
	for (size_t i = 0; i < n && src[i] != '\0'; i++) {
		*dest = src[i];
		dest++;
	}

	*dest = '\0';
	dest++;

	return dest;
}

static AOEFFSectHeader* generateSectionHeaders(SectionTable* sectTable) {
	int sectEntries = 0;
	for (int i = 0; i < 6; i++) {
		if (sectTable->entries[i].size != 0) sectEntries++;
	}
	sectEntries++; // ending blank entry

	AOEFFSectHeader* headers = (AOEFFSectHeader*) malloc(sizeof(AOEFFSectHeader) * sectEntries);
	if (!headers) emitError(ERR_MEM, NULL, "Failed to allocate memory for section headers.");

	// Offset where all sections start at, basically the end of the string table
	uint32_t baseOffset = sizeof(AOEFFheader) + (sizeof(AOEFFSectHeader) * sectEntries);

	for (int i = 0, hdrIdx = 0; i < 6; i++) {
		if (sectTable->entries[i].size == 0) continue;

		headers[hdrIdx] = (AOEFFSectHeader) {
			.shSectName = {0},
			.shSectOff = baseOffset,
			.shSectSize = sectTable->entries[i].size,
			.shSectRel = SE_SECT_UNDEF // No relocations for now
		};
		// Set section name
		const char* sectName = "";
		switch (i) {
			case DATA_SECT_N: sectName = ".data"; break;
			case CONST_SECT_N: sectName = ".const"; break;
			case BSS_SECT_N: sectName = ".bss"; break;
			case TEXT_SECT_N: sectName = ".text"; break;
			case EVT_SECT_N: sectName = ".evt"; break;
			case IVT_SECT_N: sectName = ".ivt"; break;
		}
		strncpy(headers[hdrIdx].shSectName, sectName, 8);

		baseOffset += sectTable->entries[i].size;
		hdrIdx++;
	}

	return headers;
}

static AOEFFSymbEntry* generateSymbolTable(SymbolTable* symbTable, uint32_t strTabSize, char** outStrTab) {
	uint32_t symbTableSize = symbTable->size;
	// Including empty ending symbol
	symbTableSize++;

	AOEFFSymbEntry* entries = (AOEFFSymbEntry*) malloc(sizeof(AOEFFSymbEntry) * symbTableSize);
	if (!entries) emitError(ERR_MEM, NULL, "Failed to allocate memory for symbol table.");

	// Build string table at the same time
	char* strTab = (char*) malloc(sizeof(char) * strTabSize);
	if (!strTab) emitError(ERR_MEM, NULL, "Failed to allocate memory for symbol string table.");

	char* stStrs = strTab;
	// First entry is empty
	// entries[0] = (AOEFFSymbEntry) {
	// 	.seSymbName = 0,
	// 	.seSymbSize = 0,
	// 	.seSymbVal = 0,
	// 	.seSymbInfo = 0,
	// 	.seSymbSect = SE_SECT_UNDEF
	// };
	// strTabPtr = nstrcat(strTabPtr, ""); // Empty string

	uint32_t stridx = 0;

	for (uint32_t i = 0; i < symbTable->size; i++) {
		symb_entry_t* symb = symbTable->entries[i];

		entries[i] = (AOEFFSymbEntry) {
			.seSymbName = stridx,
			.seSymbSize = symb->size,
			.seSymbVal = symb->value.val,
			.seSymbInfo = SE_SET_INFO(GET_MAIN_TYPE(symb->flags), GET_LOCALITY(symb->flags)),
			.seSymbSect = GET_SECTION(symb->flags)
		};
		stStrs = nstrcat(stStrs, symb->name);
		stridx += strlen(symb->name) + 1;
	}

	nstrncat(stStrs, "END_AOEFF_STRS\0", 16);

	// Add end blank entry
	entries[symbTableSize-1] = (AOEFFSymbEntry) {
		.seSymbName = 0,
		.seSymbSize = 0x00000000,
		.seSymbVal = 0x00000000,
		.seSymbInfo = SE_SET_INFO(0, 0),
		.seSymbSect = 0x00000000
	};

	*outStrTab = strTab;

	return entries;
}

void writeBinary(CodeGen* codegen, const char* filename) {
	initScope("writeBinary");

	FILE* outfile = fopen(filename, "wb");
	if (!outfile) emitError(ERR_IO, NULL, "Failed to open output file %s for writing.", filename);

	int sectEntries = 0;
	for (int i = 0; i < 6; i++) {
		if (codegen->sectionTable->entries[i].size != 0) sectEntries++;
	}
	sectEntries++; // ending blank entry

	uint32_t symbTableSize = codegen->symbolTable->size;
	// Including empty ending symbol
	symbTableSize++;

	uint32_t symbOff = sizeof(AOEFFheader) + (sizeof(AOEFFSectHeader) * sectEntries);
	uint32_t strTabOff = symbOff + (sizeof(AOEFFSymbEntry) * symbTableSize);
	
	uint32_t symbStrsSize = getSymbolStringsSize(codegen->symbolTable);

	// Write header info
	AOEFFheader header = {
		.hID = {AH_ID0, AH_ID1, AH_ID2, AH_ID3},
		.hType = AHT_AOBJ,
		.hEntry = 0, // No entry point for object files
		.hSectOff = sizeof(AOEFFheader),
		.hSectSize = sectEntries,
		.hSymbOff = symbOff,
		.hSymbSize = symbTableSize,
		.hStrTabOff = strTabOff,
		.hStrTabSize = symbStrsSize,
		.hRelDirOff = 0, // No relocations for now
		.hRelDirSize = 0,
	};
	fwrite(&header, sizeof(AOEFFheader), 1, outfile);

	// Write section headers
	AOEFFSectHeader* sectHeaders = generateSectionHeaders(codegen->sectionTable);
	fwrite(sectHeaders, sizeof(AOEFFSectHeader), sectEntries, outfile);
	free(sectHeaders);

	// Write symbol table
	// char* stStrs = NULL; // The string table
	AOEFFStrTab stringTable;
	stringTable.stStrs = NULL;
	AOEFFSymbEntry* symbEntries = generateSymbolTable(codegen->symbolTable, symbStrsSize, &stringTable.stStrs);
	fwrite(symbEntries, sizeof(AOEFFSymbEntry), symbTableSize, outfile);
	free(symbEntries);

	// Write string table
	fwrite(stringTable.stStrs, sizeof(char), symbStrsSize, outfile);
	free(stringTable.stStrs);

	// TODO: Write relocation table
	// TODO: Write dynamic library table and string table
	// TODO: Write import table

	// Write the payload
	for (int i = 0; i < 6; i++) {
		if (codegen->sectionTable->entries[i].size == 0) continue;
		if (i == BSS_SECT_N) continue; // Ignore bss
		if (i == DATA_SECT_N) {
			log("Writing data section...");
			fwrite(codegen->data.data, sizeof(uint8_t), codegen->data.dataCount, outfile);
		} else if (i == CONST_SECT_N) {
			log("Writing const section...");
			fwrite(codegen->consts.data, sizeof(uint8_t), codegen->consts.dataCount, outfile);
		} else if (i == TEXT_SECT_N) {
			log("Writing text section...");
			fwrite(codegen->text.instructions, sizeof(uint32_t), codegen->text.instructionCount, outfile);
		} else {
			emitError(ERR_INTERNAL, NULL, "Section %d has data but is not handled in writeBinary.", i);
		}
	}

	fclose(outfile);
}