#ifndef _DATA_TABLE_H
#define _DATA_TABLE_H

#include <stdint.h>

#include "libsecuredstring.h"
#include "ast.h"


typedef enum {
	STRING_TYPE,
	BYTES_TYPE,
	HWORDS_TYPE,
	WORDS_TYPE,
	FLOATS_TYPE,
	NONE_TYPE,
} data_t;

typedef struct DataEntry {
	data_t type;
	uint32_t addr; // Address in respect to its section
	uint32_t size; // Total size that it occupies

	// In order to not duplicate data/information, the data itself will be kept in AST form that the parser has
	// Since most of the data directives might emit multiple data pieces, it will be an array of ASTs
	// The only exception is .string since only one node represents it but nonetheless
	// Once again, the parser owns these ASTs, the data table only has a reference to them
	// Additionally, since the directive AST nodes already hold the array of ASTs for the data (except for unary and binary directive),
	// there is no need to create a new array; however, unary and binary directives do not use an array so there is still a need to create an array.
	Node** data; // The ASTs representing the data, owned by the parser
	int dataCount;
	int dataCapacity;

	SString* source;
	int linenum;
} data_entry_t;

typedef struct DataTable {
	data_entry_t** dataEntries;
	uint32_t dSize;
	uint32_t dCapacity;

	data_entry_t** constEntries;
	uint32_t cSize;
	uint32_t cCapacity;

	data_entry_t** bssEntries;
	uint32_t bSize;
	uint32_t bCapacity;

	data_entry_t** evtEntries;
	uint32_t eSize;
	uint32_t eCapacity;

	data_entry_t** ivtEntries;
	uint32_t iSize;
	uint32_t iCapacity;
} DataTable;

typedef enum {
	DATA_SECT,
	CONST_SECT,
	BSS_SECT,
	NONE_SECT,
	EVT_SECT,
	IVT_SECT
} data_sect_t;


/**
 * Initializes the data table.
 * @return The data table
 */
DataTable* initDataTable();

void deinitDataTable(DataTable* dataTable);


/**
 * Creates a data entry for the data table with the given information. Note that the source line and line number 
 * are in the node(s) representing the data, so the information can be extracted from there.
 * @param type The type of data
 * @param addr The address in which the data is located at relative to its section, aka the LP
 * @param size The total size that the data occupies in bytes
 * @param data The array of AST nodes representing the data, owned by the parser
 * @param dataCount The number of AST nodes in the data array
 * @param dataCapacity The capacity of the data array
 * @return The new data entry
 */
data_entry_t* initDataEntry(data_t type, uint32_t addr, uint32_t size, Node** data, int dataCount, int dataCapacity);

/**
 * 
 * @param dataTable 
 * @param dataEntry 
 * @param sectType 
 */
void addDataEntry(DataTable* dataTable, data_entry_t* dataEntry, data_sect_t sectType);

/**
 * Gets the data entry from a given section based on the address.
 * @param dataTable The data table
 * @param sectType The section type
 * @param addr The address of the data
 * @return The data entry
 */
data_entry_t* getDataEntry(DataTable* dataTable, data_sect_t sectType, uint32_t addr);


void displayDataTable(DataTable* dataTable);


#endif