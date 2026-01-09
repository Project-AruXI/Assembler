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


static AOEFFSectHdr* generateSectionHeaders(SectionTable* sectTable, uint32_t sectOff) {
	int sectEntries = 0;
	for (int i = 0; i < 6; i++) {
		if (sectTable->entries[i].size != 0) sectEntries++;
	}
	sectEntries++; // ending blank entry

	AOEFFSectHdr* headers = (AOEFFSectHdr*) calloc(sectEntries, sizeof(AOEFFSectHdr));
	if (!headers) emitError(ERR_MEM, NULL, "Failed to allocate memory for section headers.");

	// Offset where all sections start at, basically the end of the relocation table
	uint32_t baseOffset = sectOff;
	rlog("Base offset for all section data: 0x%x\n", baseOffset);
	uint32_t sectOffset = baseOffset;
	for (int i = 0, hdrIdx = 0; i < 6; i++) {
		if (sectTable->entries[i].size == 0) continue;

		headers[hdrIdx] = (AOEFFSectHdr) {
			.shSectName = {0},
			.shSectOff = sectOffset,
			.shSectSize = sectTable->entries[i].size,
		};

		if (i == BSS_SECT_N) headers[hdrIdx].shSectOff = 0; // BSS section has no offset in the binary

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

		log("Section %s starts at 0x%x\n", sectName, sectOffset);

		if (i != BSS_SECT_N) sectOffset += sectTable->entries[i].size;
		hdrIdx++;
	}

	return headers;
}

static AOEFFSymEnt* generateSymbolTable(SymbolTable* symbTable, uint32_t strTabSize, char** outStrTab) {
	uint32_t symbTableSize = symbTable->size;
	// Including empty ending symbol
	symbTableSize++;

	AOEFFSymEnt* entries = (AOEFFSymEnt*) malloc(sizeof(AOEFFSymEnt) * symbTableSize);
	if (!entries) emitError(ERR_MEM, NULL, "Failed to allocate memory for symbol table.");

	// Build string table at the same time
	char* strTab = (char*) malloc(sizeof(char) * strTabSize);
	if (!strTab) emitError(ERR_MEM, NULL, "Failed to allocate memory for symbol string table.");

	char* stStrs = strTab;

	uint32_t stridx = 0;

	for (uint32_t i = 0; i < symbTable->size; i++) {
		symb_entry_t* symb = symbTable->entries[i];

		uint32_t sect = GET_SECTION(symb->flags);
		if (sect == S_UNDEF) sect = SE_SECT_UNDEF; // Undefined section in AOEFF

		entries[i] = (AOEFFSymEnt) {
			.seSymbName = stridx,
			.seSymbSize = symb->size,
			.seSymbVal = symb->value.val,
			.seSymbInfo = SE_SET_INFO(GET_MAIN_TYPE(symb->flags), GET_LOCALITY(symb->flags)),
			.seSymbSect = sect
		};
		stStrs = nstrcat(stStrs, symb->name);
		stridx += strlen(symb->name) + 1;
	}

	nstrncat(stStrs, "END_AOEFF_STRS\0", 16);

	// Add end blank entry
	entries[symbTableSize-1] = (AOEFFSymEnt) {
		.seSymbName = 0,
		.seSymbSize = 0x00000000,
		.seSymbVal = 0x00000000,
		.seSymbInfo = SE_SET_INFO(0, 0),
		.seSymbSect = 0x00000000
	};

	*outStrTab = strTab;

	return entries;
}

static AOEFFTRelTab* generateRelocTables(RelocTable* relocTable, char** outRelStrTab, uint32_t* outRelStrSize, uint32_t* outRelTabCount) {
	AOEFFTRelTab* relocTables = (AOEFFTRelTab*) malloc(sizeof(AOEFFTRelTab) * 4);
	if (!relocTables) emitError(ERR_MEM, NULL, "Failed to allocate memory for relocation tables.");


	// Names are:
	// ".trel.text", ".trel.data", ".trel.const", ".trel.evt"
	// Allocate space for all four but only report what was used
	char* relStrTab = (char*) malloc( sizeof(char) * (strlen(".trel.text") + 1 + strlen(".trel.data") + 1 + strlen(".trel.const") + 1 + strlen(".trel.evt") + 1) );
	if (!relStrTab) emitError(ERR_MEM, NULL, "Failed to allocate memory for relocation string table.");

	char* rstStrs = relStrTab;
	uint32_t stridx = 0;

	uint32_t relocTableCount = 0;

	if (relocTable->textRelocTable.entryCount > 0) {
		AOEFFTRelTab* textRelTab = &relocTables[relocTableCount];
		textRelTab->relSect = TEXT_SECT_N;
		textRelTab->relTabName = stridx;
		textRelTab->relCount = relocTable->textRelocTable.entryCount;
		textRelTab->relEntries = (AOEFFTRelEnt*) malloc(sizeof(AOEFFTRelEnt) * textRelTab->relCount);
		if (!textRelTab->relEntries) emitError(ERR_MEM, NULL, "Failed to allocate memory for text relocation entries.");

		for (uint32_t i = 0; i < textRelTab->relCount; i++) {
			RelocEnt* srcEntry = relocTable->textRelocTable.entries[i];
			AOEFFTRelEnt* destEntry = &textRelTab->relEntries[i];

			*destEntry = (AOEFFTRelEnt) {
				.reOff = srcEntry->offset,
				.reSymb = srcEntry->symbolIdx,
				.reType = (uint8_t) srcEntry->type,
				.reAddend = srcEntry->addend
			};
		}

		rstStrs = nstrcat(rstStrs, ".trel.text");
		stridx += strlen(".trel.text") + 1;

		relocTableCount++;
	}

	if (relocTable->dataRelocTable.entryCount > 0) {
		AOEFFTRelTab* dataRelTab = &relocTables[relocTableCount];
		dataRelTab->relSect = DATA_SECT_N;
		dataRelTab->relTabName = stridx;
		dataRelTab->relCount = relocTable->dataRelocTable.entryCount;
		dataRelTab->relEntries = (AOEFFTRelEnt*) malloc(sizeof(AOEFFTRelEnt) * dataRelTab->relCount);
		if (!dataRelTab->relEntries) emitError(ERR_MEM, NULL, "Failed to allocate memory for data relocation entries.");

		for (uint32_t i = 0; i < dataRelTab->relCount; i++) {
			RelocEnt* srcEntry = relocTable->dataRelocTable.entries[i];
			AOEFFTRelEnt* destEntry = &dataRelTab->relEntries[i];

			*destEntry = (AOEFFTRelEnt) {
				.reOff = srcEntry->offset,
				.reSymb = srcEntry->symbolIdx,
				.reType = (uint8_t) srcEntry->type,
				.reAddend = srcEntry->addend
			};
		}

		rstStrs = nstrcat(rstStrs, ".trel.data");
		stridx += strlen(".trel.data") + 1;

		relocTableCount++;
	}

	if (relocTable->constRelocTable.entryCount > 0) {
		AOEFFTRelTab* constRelTab = &relocTables[relocTableCount];
		constRelTab->relSect = CONST_SECT_N;
		constRelTab->relTabName = stridx;
		constRelTab->relCount = relocTable->constRelocTable.entryCount;
		constRelTab->relEntries = (AOEFFTRelEnt*) malloc(sizeof(AOEFFTRelEnt) * constRelTab->relCount);
		if (!constRelTab->relEntries) emitError(ERR_MEM, NULL, "Failed to allocate memory for const relocation entries.");

		for (uint32_t i = 0; i < constRelTab->relCount; i++) {
			RelocEnt* srcEntry = relocTable->constRelocTable.entries[i];
			AOEFFTRelEnt* destEntry = &constRelTab->relEntries[i];

			*destEntry = (AOEFFTRelEnt) {
				.reOff = srcEntry->offset,
				.reSymb = srcEntry->symbolIdx,
				.reType = (uint8_t) srcEntry->type,
				.reAddend = srcEntry->addend
			};
		}

		rstStrs = nstrcat(rstStrs, ".trel.const");
		stridx += strlen(".trel.const") + 1;

		relocTableCount++;
	}

	if (relocTable->evtRelocTable.entryCount > 0) {
		AOEFFTRelTab* evtRelTab = &relocTables[relocTableCount];
		evtRelTab->relSect = EVT_SECT_N;
		evtRelTab->relTabName = stridx;
		evtRelTab->relCount = relocTable->evtRelocTable.entryCount;
		evtRelTab->relEntries = (AOEFFTRelEnt*) malloc(sizeof(AOEFFTRelEnt) * evtRelTab->relCount);
		if (!evtRelTab->relEntries) emitError(ERR_MEM, NULL, "Failed to allocate memory for evt relocation entries.");

		for (uint32_t i = 0; i < evtRelTab->relCount; i++) {
			RelocEnt* srcEntry = relocTable->evtRelocTable.entries[i];
			AOEFFTRelEnt* destEntry = &evtRelTab->relEntries[i];

			*destEntry = (AOEFFTRelEnt) {
				.reOff = srcEntry->offset,
				.reSymb = srcEntry->symbolIdx,
				.reType = (uint8_t) srcEntry->type,
				.reAddend = srcEntry->addend
			};
		}

		rstStrs = nstrcat(rstStrs, ".trel.evt");
		stridx += strlen(".trel.evt") + 1;

		relocTableCount++;
	}

	*outRelStrSize = stridx;
	*outRelStrTab = relStrTab;
	*outRelTabCount = relocTableCount;

	rlog("%d relocation tables generated.\n", relocTableCount);
	rlog("Relocation string table size: %d bytes.\n", stridx);

	return relocTables;
}

static uint32_t getRelTabSize(AOEFFTRelTab* relocTables, uint32_t relTabCount) {
	// Number of bytes that the whole relocation stuff uses
	// This means that there is the size of relSect + relTabName first
	// The after that, there is the array of entries, which is takes up number of entries (relCount) * size of each entry 
	// relCount then follows that array
	// All of this is per table with relTabCount tables

	uint32_t totalSize = 0;

	for (uint32_t i = 0; i < relTabCount; i++) {
		AOEFFTRelTab* tab = &relocTables[i];
		totalSize += sizeof(uint8_t); // relSect
		totalSize += 3; // padding
		totalSize += sizeof(uint32_t); // relTabName
		totalSize += sizeof(uint32_t); // relCount
		totalSize += 4; // padding
		totalSize += sizeof(AOEFFTRelEnt) * tab->relCount; // relEntries
	}

	return totalSize;
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

	uint32_t symbOff = sizeof(AOEFFhdr) + (sizeof(AOEFFSectHdr) * sectEntries);

	uint32_t strTabOff = symbOff + (sizeof(AOEFFSymEnt) * symbTableSize);
	uint32_t strTabSize = getSymbolStringsSize(codegen->symbolTable);

	uint32_t relStrOff = strTabOff + strTabSize;
	uint32_t relStrSize = 0;
	uint32_t relTabCount = 0; // how many relocation tables there are
	AOEFFRelStrTab relocStrTab;
	relocStrTab.rstStrs = NULL;
	AOEFFTRelTab* relocTables = generateRelocTables(codegen->relocTable, &relocStrTab.rstStrs, &relStrSize, &relTabCount);
	uint32_t relTabOff = relStrOff + relStrSize;
	uint32_t relTabSize = getRelTabSize(relocTables, relTabCount);
	rlog("relTabOff: 0x%x; relTabSize: %d\n", relTabOff, relTabSize);

	// Write header info
	AOEFFhdr header = {
		.hID = {AH_ID0, AH_ID1, AH_ID2, AH_ID3},
		.hType = AHT_AOBJ,
		.hEntry = 0, // No entry point for object files
		.hSectOff = sizeof(AOEFFhdr),
		.hSectSize = sectEntries,
		.hSymbOff = symbOff,
		.hSymbSize = symbTableSize,
		.hStrTabOff = strTabOff,
		.hStrTabSize = strTabSize,
		.hRelStrTabOff = relStrOff,
		.hRelStrTabSize = relStrSize,
		.hTRelTabOff = relTabOff,
		.hTRelTabSize = relTabCount,
		.hDRelTabOff = 0, // No dynamic stuff, so the stuff from here down is 0
		.hDRelTabSize = 0,
		.hDyLibTabOff = 0,
		.hDyLibTabSize = 0,
		.hDyLibStrTabOff = 0,
		.hDyLibStrTabSize = 0,
		.hImportTabOff = 0,
		.hImportTabSize = 0,
		.hExportTabOff = 0,
		.hExportTabSize = 0
	};
	fwrite(&header, sizeof(AOEFFhdr), 1, outfile);

	// Write section headers
	AOEFFSectHdr* sectHeaders = generateSectionHeaders(codegen->sectionTable, relTabOff + relTabSize);
	fwrite(sectHeaders, sizeof(AOEFFSectHdr), sectEntries, outfile);
	free(sectHeaders);

	// Write symbol table
	AOEFFStrTab stringTable;
	stringTable.stStrs = NULL;
	AOEFFSymEnt* symbEntries = generateSymbolTable(codegen->symbolTable, strTabSize, &stringTable.stStrs);
	fwrite(symbEntries, sizeof(AOEFFSymEnt), symbTableSize, outfile);
	free(symbEntries);

	// Write string table
	fwrite(stringTable.stStrs, sizeof(char), strTabSize, outfile);
	free(stringTable.stStrs);

	// Write relocation string table
	fwrite(relocStrTab.rstStrs, sizeof(char), relStrSize, outfile);
	free(relocStrTab.rstStrs);

	uint8_t zeroBufferPadding[4] = {0, 0, 0, 0};

	// Write (static) relocation table
	for (uint32_t i = 0; i < relTabCount; i++) {
		AOEFFTRelTab* tab = &relocTables[i];
		rlog("Writing relocation table for section %d with %d entries.\n", tab->relSect, tab->relCount);
		fwrite(&tab->relSect, sizeof(uint8_t), 1, outfile);
		fwrite(zeroBufferPadding, sizeof(uint8_t), 3, outfile); // padding
		fwrite(&tab->relTabName, sizeof(uint32_t), 1, outfile);
		fwrite(&tab->relCount, sizeof(uint32_t), 1, outfile);
		fwrite(zeroBufferPadding, sizeof(uint8_t), 4, outfile); // padding
		fwrite(tab->relEntries, sizeof(AOEFFTRelEnt), tab->relCount, outfile);
		free(tab->relEntries);
	}
	free(relocTables);

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
		} else if (i == EVT_SECT_N) {
			log("Writing evt section...");
			fwrite(codegen->evt.data, sizeof(uint8_t), codegen->evt.dataCount, outfile);
		} else {
			emitError(ERR_INTERNAL, NULL, "Section %d has data but is not handled in writeBinary.", i);
		}
	}

	fclose(outfile);
}