#ifndef _PARSER_H
#define _PARSER_H

#include "ast.h"
#include "config.h"
#include "SymbolTable.h"
#include "SectionTable.h"
#include "StructTable.h"


typedef struct ParserConfiguration {
	bool warningAsFatal;
	FLAGS8 warnings;
	FLAGS8 enhancedFeatures;
} ParserConfig;

typedef struct Parser {
	Token** tokens; // Array of tokens to parse, borrowed from lexer
	int tokenCount;
	int currentTokenIndex;

	// The parser owns the ASTs, all other references to its ASTs should not free them
	// For example, the data table will hold references to the ASTs of the data in its entries, but these references are borrowed
	Node** asts; // The top-level ASTs, each representing a logical line
	int astCount;
	int astCapacity;

	ParserConfig config;

	SectionTable* sectionTable;
	SymbolTable* symbolTable;
	StructTable* structTable;
} Parser;


Parser* initParser(Token** tokens, int tokenCount, ParserConfig config);
void deinitParser(Parser* parser);
void setTables(Parser* parser, SectionTable* sectionTable, SymbolTable* symbolTable, StructTable* structTable);

void parse(Parser* parser);

#endif