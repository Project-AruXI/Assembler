#ifndef _CODEGEN_H_
#define _CODEGEN_H_

#include <stdint.h>

#include "parser.h"
#include "SectionTable.h"
#include "SymbolTable.h"

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
} CodeGen;


/**
 * @brief 
 * @param sectionTable 
 * @param symbolTable
 * @return 
 */
CodeGen* initCodeGenerator(SectionTable* sectionTable, SymbolTable* symbolTable);
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