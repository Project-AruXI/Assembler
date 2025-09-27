#include <stdlib.h>
#include <string.h>

#include "StructTable.h"
#include "diagnostics.h"


StructTable* initStructTable() {
	StructTable* structTable = (StructTable*) malloc(sizeof(StructTable));
	if (!structTable) emitError(ERR_MEM, NULL, "Failed to allocate memory for struct table.");

	structTable->structs = (struct_root_t**) malloc(sizeof(struct_root_t*) * 4);
	if (!structTable->structs) emitError(ERR_MEM, NULL, "Failed to allocate memory for struct table entries.");

	structTable->size = 0;
	structTable->capacity = 4;

	return structTable;
}

void deinitStructTable(StructTable* structTable) {
	for (int i = 0; i < structTable->size; i++) {
		deinitStruct(structTable->structs[i]);
	}
	free(structTable->structs);
	free(structTable);
}

struct_root_t* initStruct(const char* name) {
	struct_root_t* structDef = (struct_root_t*) malloc(sizeof(struct_root_t));
	if (!structDef) emitError(ERR_MEM, NULL, "Failed to allocate memory for struct definition.");

	structDef->name = strdup(name);
	if (!structDef->name) emitError(ERR_MEM, NULL, "Failed to allocate memory for struct name.");

	structDef->size = 0;

	structDef->fields = (struct_field_t**) malloc(sizeof(struct_field_t*) * 4);
	if (!structDef->fields) emitError(ERR_MEM, NULL, "Failed to allocate memory for struct fields.");
	structDef->fieldCount = 0;
	structDef->fieldCapacity = 4;

	return structDef;
}

void deinitStruct(struct_root_t* structDef) {
	for (int i = 0; i < structDef->fieldCount; i++) {
		deinitStructField(structDef->fields[i]);
	}
	free(structDef->fields);
	free(structDef->name);
	free(structDef);
}

struct_field_t* initStructField(const char* name, structFieldType type, int size, int offset, int structTypeIdx) {
	struct_field_t* field = (struct_field_t*) malloc(sizeof(struct_field_t));
	if (!field) emitError(ERR_MEM, NULL, "Failed to allocate memory for struct field.");

	field->name = strdup(name);
	if (!field->name) emitError(ERR_MEM, NULL, "Failed to allocate memory for struct field name.");
	field->type = type;
	field->size = size;
	field->offset = offset;
	field->structTypeIdx = structTypeIdx;

	return field;
}

void deinitStructField(struct_field_t* field) {
	free(field->name);
	free(field);
}

int addStruct(StructTable* structTable, struct_root_t* structDef) {
	if (structTable->size == structTable->capacity) {
		structTable->capacity += 2;
		struct_root_t** temp = (struct_root_t**) realloc(structTable->structs, sizeof(struct_root_t*) * structTable->capacity);
		if (!temp) emitError(ERR_MEM, NULL, "Failed to reallocate memory for struct table entries.");
		structTable->structs = temp;
	}
	structTable->structs[structTable->size++] = structDef;

	return structTable->size - 1;
}

struct_root_t* getStructByName(StructTable* structTable, const char* name) {
	for (int i = 0; i < structTable->size; i++) {
		if (strcmp(structTable->structs[i]->name, name) == 0) return structTable->structs[i];
	}

	return NULL;
}

struct_root_t* getStructByIndex(StructTable* structTable, int index) {
	if (structTable->size == 0) emitError(ERR_INTERNAL, NULL, "Struct table is empty.");
	if (index < 0 || index >= structTable->size) emitError(ERR_INTERNAL, NULL, "Struct index %d out of bounds (size: %d).", index, structTable->size);

	return structTable->structs[index];
}

bool addStructField(struct_root_t* structDef, struct_field_t* field) {
	if (hasStructField(structDef, field->name)) return false;

	if (structDef->fieldCount == structDef->fieldCapacity) {
		structDef->fieldCapacity += 2;
		struct_field_t** temp = (struct_field_t**) realloc(structDef->fields, sizeof(struct_field_t*) * structDef->fieldCapacity);
		if (!temp) emitError(ERR_MEM, NULL, "Failed to reallocate memory for struct fields.");
		structDef->fields = temp;
	}
	structDef->fields[structDef->fieldCount++] = field;
	structDef->size += field->size;

	return true;
}

bool hasStructField(struct_root_t* structDef, const char* fieldName) {
	for (int i = 0; i < structDef->fieldCount; i++) {
		if (strcmp(structDef->fields[i]->name, fieldName) == 0) return true;
	}

	return false;
}

struct_field_t* getStructFieldByName(struct_root_t* structDef, const char* fieldName) {
	for (int i = 0; i < structDef->fieldCount; i++) {
		if (strcmp(structDef->fields[i]->name, fieldName) == 0) return structDef->fields[i];
	}

	return NULL;
}

void displayStructTable(StructTable* structTable) {
	if (structTable->size == 0) {
		rtrace("\n[Struct Table is empty]\n");
		return;
	}
	rtrace("\n==================== Struct Table ====================");
	rtrace("Total Structs: %d", structTable->size);
	rtrace("-----------------------------------------------------");
	rtrace("| %-3s | %-20s | %-6s | %-6s |", "#", "Name", "Fields", "Size");
	rtrace("-----------------------------------------------------");
	for (int i = 0; i < structTable->size; i++) {
		struct_root_t* s = structTable->structs[i];
		rtrace("| %-3d | %-20s | %-6d | %-6d |", i, s->name, s->fieldCount, s->size);
	}
	rtrace("-----------------------------------------------------\n");
}

void displayStruct(struct_root_t* structDef) {
	if (!structDef) {
		rtrace("[Null struct definition]\n");
		return;
	}
	rtrace("\n==================== Struct Definition ====================");
	rtrace("Name: %s", structDef->name);
	rtrace("Size: %d bytes", structDef->size);
	rtrace("Fields: %d", structDef->fieldCount);
	rtrace("----------------------------------------------------------");
	rtrace("| %-3s | %-16s | %-8s | %-6s | %-6s |", "#", "Field Name", "Type", "Size", "Offset");
	rtrace("----------------------------------------------------------");
	for (int i = 0; i < structDef->fieldCount; i++) {
		struct_field_t* f = structDef->fields[i];
		rtrace("| %-3d | %-16s | %-8d | %-6d | %-6d |", i, f->name, f->type, f->size, f->offset);
	}
	rtrace("----------------------------------------------------------\n");
}