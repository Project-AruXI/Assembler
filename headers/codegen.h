#ifndef _CODEGEN_H_
#define _CODEGEN_H_

#include <stdint.h>

#include "parser.h"
#include "SectionTable.h"
#include "SymbolTable.h"
#include "RelocTable.h"

typedef struct CodeGenerator {
	struct {
		uint32_t* instructions;
		int instructionCount;
		int instructionCapacity;
	} text;

	struct {
		uint8_t* data;
		int dataCount;
		int dataCapacity;
	} data;

	struct {
		uint8_t* data;
		int dataCount;
		int dataCapacity;
	} consts;

	SectionTable* sectionTable;
	SymbolTable* symbolTable;
	RelocTable* relocTable;
} CodeGen;


/**
 * @brief 
 * @param sectionTable 
 * @param symbolTable
 * @param relocTable
 * @return 
 */
CodeGen* initCodeGenerator(SectionTable* sectionTable, SymbolTable* symbolTable, RelocTable* relocTable);
/**
 * @brief 
 * @param codegen 
 */
void deinitCodeGenerator(CodeGen* codegen);

/**
 * @brief 
 * @param parser 
 * @param codegen 
 */
void gencode(Parser* parser, CodeGen* codegen);

void displayCodeGen(CodeGen* codegen);


/**
 * @brief 
 * @param codegen 
 * @param filename 
 */
void writeBinary(CodeGen* codegen, const char* filename);



#endif