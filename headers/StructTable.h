#ifndef _STRUCT_TABLE_H_
#define _STRUCT_TABLE_H_

#include <stdbool.h>

#include "libsecuredstring.h"

typedef enum {
	BYTE_FT,
	HWORD_FT,
	WORD_FT,
	STRUCT_FT,
	UNION_FT
} structFieldType;

typedef struct StructField {
	char* name;
	structFieldType type;
	int size; // The size (how many bits in takes up) of the field
	int offset; // The offset of the field from the start
	int structTypeIdx; // If field is struct, the index of that type in the struct table

	SString* source; // The source file where the field was defined
	int linenum; // The line number where the field was defined
} struct_field_t;

typedef struct StructRoot {
	char* name; // The name of the struct
	int size; // The total size of the struct in bytes

	struct_field_t** fields; // The struct fields
	int fieldCount;
	int fieldCapacity;

	SString* source; // The source file where the struct was defined
	int linenum; // The line number where the struct was defined

	int index; // The index of this struct in the table
} struct_root_t;

typedef struct StructDefTable {
	struct_root_t** structs;
	int size;
	int capacity;
} StructTable;


StructTable* initStructTable();
void deinitStructTable(StructTable* structTable);

struct_root_t* initStruct(const char* name);
void deinitStruct(struct_root_t* structDef);

struct_field_t* initStructField(const char* name, structFieldType type, int size, int offset, int structTypeIdx);
void deinitStructField(struct_field_t* field);

int addStruct(StructTable* structTable, struct_root_t* structDef);
bool addStructField(struct_root_t* structDef, struct_field_t* field);

bool hasStructField(struct_root_t* structDef, const char* fieldName);

struct_root_t* getStructByName(StructTable* structTable, const char* name);
struct_root_t* getStructByIndex(StructTable* structTable, int index);
struct_field_t* getStructFieldByName(struct_root_t* structDef, const char* fieldName);

void displayStructTable(StructTable* structTable);
void displayStruct(struct_root_t* structDef);

#endif