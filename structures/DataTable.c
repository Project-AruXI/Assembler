#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DataTable.h"
#include "diagnostics.h"


DataTable* initDataTable() {
	DataTable* dataTable = (DataTable*) malloc(sizeof(DataTable));
	if (!dataTable) emitError(ERR_MEM, NULL, "Could not allocate memory for data table!\n");

	dataTable->dataEntries = (data_entry_t**) malloc(sizeof(data_entry_t*) * 5);
	dataTable->dSize = 0;
	dataTable->dCapacity = 5;

	dataTable->constEntries = (data_entry_t**) malloc(sizeof(data_entry_t*) * 5);
	dataTable->cSize = 0;
	dataTable->cCapacity = 5;

	dataTable->bssEntries = (data_entry_t**) malloc(sizeof(data_entry_t*) * 5);
	dataTable->bSize = 0;
	dataTable->bCapacity = 5;

	dataTable->evtEntries = (data_entry_t**) malloc(sizeof(data_entry_t*) * 5);
	dataTable->eSize = 0;
	dataTable->eCapacity = 5;

	dataTable->ivtEntries = (data_entry_t**) malloc(sizeof(data_entry_t*) * 5);
	dataTable->iSize = 0;
	dataTable->iCapacity = 5;

	return dataTable;
}

data_entry_t* initDataEntry(data_t type, uint32_t addr, uint32_t size, Node** data, int dataCount, int dataCapacity) {
	data_entry_t* dataEntry = (data_entry_t*) malloc(sizeof(data_entry_t));
	if (!dataEntry) emitError(ERR_MEM, NULL, "Could not allocate space for data entry!\n");

	dataEntry->type = type;
	dataEntry->addr = addr;
	dataEntry->size = size;
	
	dataEntry->data = data;
	dataEntry->dataCount = dataCount;
	dataEntry->dataCapacity = dataCapacity;

	// The source line string and line number is in a node, get it from the first node
	Node* firstNode = data[0];
	// This should not happen as the array must always be at least of size 1
	if (!firstNode) emitError(ERR_INTERNAL, NULL, "Data entry has no data nodes!\n");

	dataEntry->source = ssCreateSecuredString(ssGetString(firstNode->token->sstring));
	dataEntry->linenum = firstNode->token->linenum;

	return dataEntry;
}

void addDataEntry(DataTable* dataTable, data_entry_t* dataEntry, data_sect_t sectType) {
	data_entry_t*** entries = NULL;
	uint32_t* size = 0;
	uint32_t* capacity = 0;

	trace("Detected section to add for: %d\n", sectType);
	if (sectType == DATA_SECT) {
		entries = &dataTable->dataEntries;
		size = &dataTable->dSize;
		capacity = &dataTable->dCapacity;
	} else if (sectType == CONST_SECT) {
		entries = &dataTable->constEntries;
		size = &dataTable->cSize;
		capacity = &dataTable->cCapacity;
	} else if (sectType == BSS_SECT) {
		entries = &dataTable->bssEntries;
		size = &dataTable->bSize;
		capacity = &dataTable->bCapacity;
	} else if (sectType == EVT_SECT) {
		entries = &dataTable->evtEntries;
		size = &dataTable->eSize;
		capacity = &dataTable->eCapacity;
	} else if (sectType == IVT_SECT) {
		entries = &dataTable->ivtEntries;
		size = &dataTable->iSize;
		capacity = &dataTable->iCapacity;
	}	else return;

	if (*size == *capacity) {
		*capacity *= 2;

		data_entry_t** temp = (data_entry_t**) realloc(*entries, sizeof(data_entry_t*) * (*capacity));
		if (!temp) emitError(ERR_MEM, NULL, "Could not reallocate memory for data entries!\n");

		*entries = temp;
	}

	int idx = *size;
	(*entries)[idx] = dataEntry;
	*size = *size + 1;
}

data_entry_t* getDataEntry(DataTable* dataTable, data_sect_t sectType, uint32_t addr) {
	data_entry_t** entries = NULL;
	uint32_t size = 0;

	if (sectType == DATA_SECT) { entries = dataTable->dataEntries; size = dataTable->dSize; }
	else if (sectType == CONST_SECT) { entries = dataTable->constEntries; size = dataTable->cSize; }
	else if (sectType == BSS_SECT) { entries = dataTable->bssEntries; size = dataTable->bSize; }
	else if (sectType == EVT_SECT) { entries = dataTable->evtEntries; size = dataTable->eSize; }
	else if (sectType == IVT_SECT) { entries = dataTable->ivtEntries; size = dataTable->iSize; }
	else return NULL;


	for (int i = 0; i < size; i++) {
		data_entry_t* entry = entries[i];

		if (entry->addr == addr) return entry;
	}

	return NULL;
}

static char* typeToString(data_t type) {
	switch (type) {
		case STRING_TYPE: return "STRING";
		case BYTES_TYPE:  return "BYTES";
		case HWORDS_TYPE: return "HWORDS";
		case WORDS_TYPE:  return "WORDS";
		case FLOATS_TYPE: return "FLOATS";
		default:          return "UNKNOWN";
	}
}

static void displayDataEntry(data_entry_t* dataEntry) {
	if (!dataEntry) {
		rtrace("[Null data entry]\n");
		return;
	}
	rtrace("---------------- Data Entry ----------------");
	rtrace("Addr:   0x%08x", dataEntry->addr);
	rtrace("Size:   %-6u bytes", dataEntry->size);
	rtrace("Type:   %-8s", typeToString(dataEntry->type));
	rtrace("Line:   %-5d", dataEntry->linenum);
	rtrace("Source: %s", ssGetString(dataEntry->source));
	rtrace("Data count: %d", dataEntry->dataCount);
	rtrace("--------------------------------------------\n");
	// TODO: Print the actual data represented by the AST nodes
	// This can only be done when the AST nodes are evaluated/resolved into actual values
	//  and the complete data is shown in the root of the expression
}

void displayDataTable(DataTable* dataTable) {
	if (!dataTable) {
		rtrace("[Data Table is null]\n");
		return;
	}
	const char* sectionNames[] = {"DATA", "CONST", "BSS", "EVT", "IVT"};
	data_entry_t** entriesArr[] = {
		dataTable->dataEntries, dataTable->constEntries, dataTable->bssEntries, dataTable->evtEntries, dataTable->ivtEntries
	};
	uint32_t sizes[] = {dataTable->dSize, dataTable->cSize, dataTable->bSize, dataTable->eSize, dataTable->iSize};

	for (int s = 0; s < 5; ++s) {
		rtrace("\n==================== %-5s Section ====================", sectionNames[s]);
		rtrace("Total Entries: %u", sizes[s]);
		rtrace("-----------------------------------------------------------------------------------------------------");
		rtrace("| %-4s | %-10s | %-12s | %-10s | %-6s | %-40s |", "#", "Address", "Size (bytes)", "Type", "Line", "Source");
		rtrace("-----------------------------------------------------------------------------------------------------");
		for (uint32_t i = 0; i < sizes[s]; ++i) {
			data_entry_t* entry = entriesArr[s][i];
			const char* src = ssGetString(entry->source);
			char truncated[41];
			int len = strlen(src);
			if (len > 40) {
				strncpy(truncated, src, 37);
				truncated[37] = '.';
				truncated[38] = '.';
				truncated[39] = '.';
				truncated[40] = '\0';
				src = truncated;
			}
			rtrace("| %-4u | 0x%08x | %-12u | %-10s | %-6d | %-40s |", i, entry->addr, entry->size, typeToString(entry->type), entry->linenum, src);
		}
		rtrace("-----------------------------------------------------------------------------------------------------\n");
		// Print details for each entry after the overview table
		for (uint32_t i = 0; i < sizes[s]; ++i) {
			displayDataEntry(entriesArr[s][i]);
		}
	}
}

void deinitDataTable(DataTable* dataTable) {
	for (int i = 0; i < dataTable->dSize; i++) {
		data_entry_t* entry = dataTable->dataEntries[i];

		if (entry->source) free(entry->source);
		free(entry);
	}
	free(dataTable->dataEntries);

	for (int i = 0; i < dataTable->cSize; i++) {
		data_entry_t* entry = dataTable->constEntries[i];

		if (entry->source) free(entry->source);
		free(entry);
	}
	free(dataTable->constEntries);

	for (int i = 0; i < dataTable->bSize; i++) {
		data_entry_t* entry = dataTable->bssEntries[i];

		if (entry->source) free(entry->source);
		free(entry);
	}
	free(dataTable->bssEntries);

	for (int i = 0; i < dataTable->eSize; i++) {
		data_entry_t* entry = dataTable->evtEntries[i];

		if (entry->source) free(entry->source);
		free(entry);
	}
	free(dataTable->evtEntries);

	for (int i = 0; i < dataTable->iSize; i++) {
		data_entry_t* entry = dataTable->ivtEntries[i];

		if (entry->source) free(entry->source);
		free(entry);
	}
	free(dataTable->ivtEntries);

	free(dataTable);
}