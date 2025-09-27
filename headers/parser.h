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

	Node** asts;
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