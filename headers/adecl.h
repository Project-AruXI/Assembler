#ifndef _ADECL_H_
#define _ADECL_H_

#include <stdio.h>

#include "ast.h"
#include "sds.h"
#include "parser.h"
#include "SymbolTable.h"
#include "StructTable.h"


typedef struct ADECLContext {
	// The "module" needs to be aware of the config and data that the assembler is working with
	// So it can add symbols to the symbol table and structs to the struct table, and report back
	// The ASTs it creates will be added to the parser's AST list, so that needs to be returned
	ParserConfig parentParserConfig;
	SymbolTable* symbolTable;
	StructTable* structTable;
	// Note that data directives are not allowed in ADECL files, so the data table is not needed here
	// Same with section table as there is no concept of sections in ADECL files
	Node** asts; // The ASTs created from the ADECL file, the parent parser now takes ownership of these
	int astCount;
	int astCapacity;
} ADECL_ctx;

FILE* openADECLFile(sds filename);
void lexParseADECLFile(FILE* file, ADECL_ctx* context);

#endif