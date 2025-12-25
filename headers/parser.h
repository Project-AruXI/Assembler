#ifndef _PARSER_H
#define _PARSER_H

#include "ast.h"
#include "config.h"
#include "SymbolTable.h"
#include "SectionTable.h"
#include "StructTable.h"
#include "DataTable.h"
#include "RelocTable.h"


struct LDIMM {
	Node* ldInstr;
	struct LDIMM* next;
};


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

	bool processing; // In case of a .end directive, stop processing further lines

	struct LDIMM* ldimmList; // Linked list of ld immediate/move possible decompositions to process
	struct LDIMM* ldimmTail;

	SectionTable* sectionTable;
	SymbolTable* symbolTable;
	StructTable* structTable;
	DataTable* dataTable;
	RelocTable* relocTable;
} Parser;


Parser* initParser(Token** tokens, int tokenCount, ParserConfig config);
void deinitParser(Parser* parser);
void setTables(Parser* parser, SectionTable* sectionTable, SymbolTable* symbolTable, StructTable* structTable, DataTable* dataTable, RelocTable* relocTable);

void parse(Parser* parser);

void showParserConfig(Parser* parser);

/**
 * Adds an ld immediate/move instruction decomposition to the parser's list for quick access.
 * This allows the parser to keep track of all such instructions that may need to be decomposed later.
 * @param parser The parser
 * @param ldInstrNode The AST node of the ld instruction
 */
void addLD(Parser* parser, Node* ldInstrNode);

#endif