#include <ctype.h>

#include "handlers.h"
#include "diagnostics.h"
#include "expr.h"
#include "reserved.h"
#include "adecl.h"
#include "StructTable.h"


void handleData(Parser* parser) {
	// `.data` must not be followed by anything on the same line
	Token* directiveToken = parser->tokens[parser->currentTokenIndex++];
	linedata_ctx linedata = {
		.linenum = directiveToken->linenum,
		.source = ssGetString(directiveToken->sstring)
	};

	log("Handling .data directive at line %d", directiveToken->linenum);

	Token* nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type != TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.data` directive must not be followed by anything on the same line.");

	parser->currentTokenIndex++; // Consume the newline

	// Set the current section
	parser->sectionTable->activeSection = DATA_SECT_N;
}

void handleConst(Parser* parser) {
	// `.const` must not be followed by anything on the same line
	Token* directiveToken = parser->tokens[parser->currentTokenIndex++];
	linedata_ctx linedata = {
		.linenum = directiveToken->linenum,
		.source = ssGetString(directiveToken->sstring)
	};

	log("Handling .const directive at line %d", directiveToken->linenum);

	Token* nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type != TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.const` directive must not be followed by anything on the same line.");

	parser->currentTokenIndex++;

	parser->sectionTable->activeSection = CONST_SECT_N;
}

void handleBss(Parser* parser) {
	// `.bss` must not be followed by anything on the same line
	Token* directiveToken = parser->tokens[parser->currentTokenIndex++];
	linedata_ctx linedata = {
		.linenum = directiveToken->linenum,
		.source = ssGetString(directiveToken->sstring)
	};

	log("Handling .bss directive at line %d", directiveToken->linenum);

	Token* nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type != TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.bss` directive must not be followed by anything on the same line.");

	parser->currentTokenIndex++;

	parser->sectionTable->activeSection = BSS_SECT_N;
}

void handleText(Parser* parser) {
	// `.text` must not be followed by anything on the same line
	Token* directiveToken = parser->tokens[parser->currentTokenIndex++];
	linedata_ctx linedata = {
		.linenum = directiveToken->linenum,
		.source = ssGetString(directiveToken->sstring)
	};

	log("Handling .text directive at line %d", directiveToken->linenum);

	Token* nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type != TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.text` directive must not be followed by anything on the same line.");

	parser->currentTokenIndex++;

	parser->sectionTable->activeSection = TEXT_SECT_N;
}

void handleEvt(Parser* parser) {
	// `.evt` must not be followed by anything on the same line
	Token* directiveToken = parser->tokens[parser->currentTokenIndex++];
	linedata_ctx linedata = {
		.linenum = directiveToken->linenum,
		.source = ssGetString(directiveToken->sstring)
	};

	log("Handling .evt directive at line %d", directiveToken->linenum);

	Token* nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type != TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.evt` directive must not be followed by anything on the same line.");

	parser->currentTokenIndex++;

	parser->sectionTable->activeSection = EVT_SECT_N;
}

void handleIvt(Parser* parser) {
	// `.ivt` must not be followed by anything on the same line
	Token* directiveToken = parser->tokens[parser->currentTokenIndex++];
	linedata_ctx linedata = {
		.linenum = directiveToken->linenum,
		.source = ssGetString(directiveToken->sstring)
	};

	log("Handling .ivt directive at line %d", directiveToken->linenum);

	Token* nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type != TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.ivt` directive must not be followed by anything on the same line.");

	parser->currentTokenIndex++;

	parser->sectionTable->activeSection = IVT_SECT_N;
}


static void validateSymbolToken(Token* token, linedata_ctx* linedata) {
	// Make sure the symbol is valid
	if (token->lexeme[0] != '_' && !isalpha(token->lexeme[0])) {
		emitError(ERR_INVALID_LABEL, linedata, "Symbol must start with an alphabetic character or underscore: `%s`", token->lexeme);
	}

	// Also make sure the symbol is not a reserved word
	if (indexOf(REGISTERS, sizeof(REGISTERS)/sizeof(REGISTERS[0]), token->lexeme) != -1 ||
			indexOf(DIRECTIVES, sizeof(DIRECTIVES)/sizeof(DIRECTIVES[0]), token->lexeme) != -1 ||
			indexOf(INSTRUCTIONS, sizeof(INSTRUCTIONS)/sizeof(INSTRUCTIONS[0]), token->lexeme) != -1) {
		emitError(ERR_INVALID_LABEL, linedata, "Symbol cannot be a reserved word: `%s`", token->lexeme);
	}
}


void handleSet(Parser* parser, Node* directiveRoot) {
	initScope("handleSet");

	Token* directiveToken = parser->tokens[parser->currentTokenIndex++];
	linedata_ctx linedata = {
		.linenum = directiveToken->linenum,
		.source = ssGetString(directiveToken->sstring)
	};

	directiveToken->type = TK_D_SET;

	DirctvNode* directiveData = initDirectiveNode();
	setNodeData(directiveRoot, directiveData, ND_DIRECTIVE);

	log("Handling .set directive at line %d", directiveToken->linenum);

	// The directive is in the form of `.set symbol, expr`

	Token* nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type == TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.set` directive must be followed by a symbol and an expression.");
	if (nextToken->type != TK_IDENTIFIER) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.set` directive must be followed by an identifier, got `%s`.", nextToken->lexeme);
	// Since the lexer bunched up many things under TK_IDENTIFIER, it needs to be checked whether the token is actually a symbol
	// What constitutes a symbol is the same as for a label, just reuse the logic
	validateSymbolToken(nextToken, &linedata);
	Token* symbToken = nextToken;

	// Ensure that the symbol has not been defined
	// Note that this means an entry can exist but it is only in the case that it has been referenced before
	// If it has been referenced, just updated the defined status

	symb_entry_t* symbEntry = getSymbolEntry(parser->symbolTable, nextToken->lexeme);
	if (symbEntry && GET_DEFINED(symbEntry->flags)) {
		emitError(ERR_REDEFINED, &linedata, "Symbol redefinition: `%s`. First defined at `%s`", nextToken->lexeme, ssGetString(symbEntry->source));
	}

	parser->currentTokenIndex++; // Consume the symbol token
	// Make sure comma is next
	nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type != TK_COMMA) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.set` directive must have a comma after the symbol.");
	parser->currentTokenIndex++; // Consume the comma

	// Onwards to the expression
	Node* exprRoot = parseExpression(parser);
	exprRoot->astNodeType = AST_INTERNAL;
	exprRoot->parent = directiveRoot;

	// Assume currentTokenIndex was updated in parseExpression for now
	nextToken = parser->tokens[parser->currentTokenIndex]; // This better be the newline
	if (nextToken->type != TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.set` directive must be followed by only a symbol and an expression on the same line.");
	parser->currentTokenIndex++; // Consume the newline

	int symbTableIndex = -1;
	// Finish dealing with the symbol table
	if (symbEntry) {
		// Update the existing entry to be defined now
		SET_DEFINED(symbEntry->flags);
		symbEntry->value.expr = exprRoot;
	} else {
		SYMBFLAGS flags = CREATE_FLAGS(M_ABS, T_NONE, E_EXPR, parser->sectionTable->activeSection, L_LOC, R_NREF, D_DEF);
		symbEntry = initSymbolEntry(symbToken->lexeme, flags, exprRoot, 0, symbToken->sstring, symbToken->linenum);
		
		addSymbolEntry(parser->symbolTable, symbEntry);
		symbTableIndex = parser->symbolTable->size - 1;
	}

	Node* symbNode = initASTNode(AST_LEAF, ND_SYMB, symbToken, directiveRoot);
	SymbNode* symbData = initSymbolNode(symbTableIndex, 0);
	setNodeData(symbNode, symbData, ND_SYMB);
	setBinaryDirectiveData(directiveData, symbNode, exprRoot);
}

void handleGlob(Parser* parser, Node* directiveRoot) {
	initScope("handleGlob");

	Token* directiveToken = parser->tokens[parser->currentTokenIndex++];
	linedata_ctx linedata = {
		.linenum = directiveToken->linenum,
		.source = ssGetString(directiveToken->sstring)
	};

	directiveToken->type = TK_D_GLOB;

	DirctvNode* directiveData = initDirectiveNode();
	setNodeData(directiveRoot, directiveData, ND_DIRECTIVE);

	log("Handling .glob directive at line %d", directiveToken->linenum);

	// The directive is in the form of `.glob symbol`

	Token* nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type == TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.glob` directive must be followed by a symbol.");
	if (nextToken->type != TK_IDENTIFIER) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.glob` directive must be followed by an identifier, got `%s`.", nextToken->lexeme);
	// Same thing as .set
	validateSymbolToken(nextToken, &linedata);
	Token* symbToken = nextToken;

	// If the symbol exists, just update it to be global no matter what (defined or undefined)
	// Otherwise, create a new entry

	int symbTableIndex = -1;
	symb_entry_t* symbEntry = getSymbolEntry(parser->symbolTable, nextToken->lexeme);
	if (symbEntry) SET_LOCALITY(symbEntry->flags);
	else {
		// Since .glob only sets the locality status, and this is its first sighting, not much is known
		// Just go with defaults/assumptions that it is absolute, no type, and value
		// The assumption is that most commonly, addresses are made global
		SYMBFLAGS flags = CREATE_FLAGS(M_ABS, T_NONE, E_VAL, parser->sectionTable->activeSection, L_GLOB, R_REF, D_UNDEF);
		symbEntry = initSymbolEntry(symbToken->lexeme, flags, NULL, 0, symbToken->sstring, symbToken->linenum);
		addSymbolEntry(parser->symbolTable, symbEntry);
		symbTableIndex = parser->symbolTable->size - 1;
	}
	addSymbolReference(symbEntry, symbToken->sstring, symbToken->linenum);

	// Make sure there is nothing else afterwards except for newline
	parser->currentTokenIndex++; // Consume the symbol token
	nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type != TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.glob` directive must be followed by only a symbol on the same line.");
	parser->currentTokenIndex++; // Consume the newline

	Node* symbNode = initASTNode(AST_LEAF, ND_SYMB, symbToken, directiveRoot);
	SymbNode* symbData = initSymbolNode(symbTableIndex, 0);
	setNodeData(symbNode, symbData, ND_SYMB);
	setUnaryDirectiveData(directiveData, symbNode);
}

static void validateSection(Parser* parser, Token* directive, linedata_ctx* linedata) {
	// .set, .glob, .end, .align, .size, .extern, .type, .include, .def, .sizeof, and .typeinfo
	//  are allowed anywhere
	// .string, .byte, .hword, .word, .float, .zero, .fill are allowed in data and const minimum
	// .byte, .hword, .word, and .zero are allowed in evt and ivt
	// .zero is allowed in bss

	int directiveType = directive->type;
	int activeSection = parser->sectionTable->activeSection;
	char* sectionStr = NULL;
	switch (activeSection) {
		case DATA_SECT_N: sectionStr = ".data"; break;
		case CONST_SECT_N: sectionStr = ".const"; break;
		case BSS_SECT_N: sectionStr = ".bss"; break;
		case TEXT_SECT_N: sectionStr = ".text"; break;
		case EVT_SECT_N: sectionStr = ".evt"; break;
		case IVT_SECT_N: sectionStr = ".ivt"; break;
		default: sectionStr = "unknown"; break;
	}

	if (directiveType == TK_D_STRING || directiveType == TK_D_FLOAT) {
		if (activeSection != DATA_SECT_N && activeSection != CONST_SECT_N) {
			emitError(ERR_DIRECTIVE_NOT_ALLOWED, linedata, "The `%s` directive is not allowed in the %s section.", ssGetString(directive->sstring), sectionStr);
		}
	} else if (directiveType == TK_D_BYTE || directiveType == TK_D_HWORD || directiveType == TK_D_WORD) {
		if (activeSection != DATA_SECT_N && activeSection != CONST_SECT_N &&
				activeSection != EVT_SECT_N && activeSection != IVT_SECT_N) {
			emitError(ERR_DIRECTIVE_NOT_ALLOWED, linedata, "The `%s` directive is not allowed in the %s section.", ssGetString(directive->sstring), sectionStr);
		}
	} else if (directiveType == TK_D_ZERO) {
		if (activeSection != DATA_SECT_N && activeSection != CONST_SECT_N &&
				activeSection != BSS_SECT_N &&
				activeSection != EVT_SECT_N && activeSection != IVT_SECT_N) {
			emitError(ERR_DIRECTIVE_NOT_ALLOWED, linedata, "The `%s` directive is not allowed in the %s section.", ssGetString(directive->sstring), sectionStr);
		}
		if (activeSection != BSS_SECT_N) {
			emitWarning(WARN_UNEXPECTED, linedata, "Consider using the `.zero` directive in the `.bss` section instead of `%s`.", sectionStr);
		}
	}
}

void handleString(Parser* parser, Node* directiveRoot) {
	initScope("handleString");

	Token* directiveToken = parser->tokens[parser->currentTokenIndex++];
	linedata_ctx linedata = {
		.linenum = directiveToken->linenum,
		.source = ssGetString(directiveToken->sstring)
	};

	directiveToken->type = TK_D_STRING;

	validateSection(parser, directiveToken, &linedata);

	DirctvNode* directiveData = initDirectiveNode();
	setNodeData(directiveRoot, directiveData, ND_DIRECTIVE);

	log("Handling .string directive at line %d", directiveToken->linenum);

	// The directive is in the form of `.string "some string"`

	Token* nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type == TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.string` directive must be followed by a string.");
	if (nextToken->type != TK_STRING) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.string` directive must be followed by a string, got `%s`.", nextToken->lexeme);

	// Although there is only the need to hold a single string, the way that the data table is set up, it requires an array
	Node** stringArray = newNodeArray(1);
	if (!stringArray) emitError(ERR_MEM, NULL, "Failed to allocate memory for `.string` directive string.");
	int stringArrayCapacity = 1;
	int stringArrayCount = 0;

	Node* stringNode = initASTNode(AST_LEAF, ND_STRING, nextToken, directiveRoot);
	setUnaryDirectiveData(directiveData, stringNode);

	stringArray = nodeArrayInsert(stringArray, &stringArrayCapacity, &stringArrayCount, stringNode);
	if (!stringArray) emitError(ERR_MEM, NULL, "Failed to reallocate memory for `.string` directive string.");

	StrNode* strData = initStringNode(nextToken->lexeme, sdslen(nextToken->lexeme));
	setNodeData(stringNode, strData, ND_STRING);

	// Need to create the data table entry
	uint32_t dataAddr = parser->sectionTable->entries[parser->sectionTable->activeSection].lp;
	uint32_t dataSize = strData->length + 1; // +1 for the null terminator
	// Set the data for the data table
	data_entry_t* stringDataEntry = initDataEntry(STRING_TYPE, dataAddr, dataSize, stringArray, stringArrayCount, stringArrayCapacity);
	parser->sectionTable->entries[parser->sectionTable->activeSection].lp += dataSize;
	addDataEntry(parser->dataTable, stringDataEntry, parser->sectionTable->activeSection);

	// Need to make sure there is nothing else afterwards except for newline

	parser->currentTokenIndex++; // Consume the string token
	nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type != TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.string` directive must be followed by only a string on the same line.");
	parser->currentTokenIndex++; // Consume the newline
}

void handleByte(Parser* parser, Node* directiveRoot) {
	initScope("handleByte");

	Token* directiveToken = parser->tokens[parser->currentTokenIndex++];
	linedata_ctx linedata = {
		.linenum = directiveToken->linenum,
		.source = ssGetString(directiveToken->sstring)
	};

	directiveToken->type = TK_D_BYTE;

	validateSection(parser, directiveToken, &linedata);

	DirctvNode* directiveData = initDirectiveNode();
	setNodeData(directiveRoot, directiveData, ND_DIRECTIVE);

	log("Handling .byte directive at line %d", directiveToken->linenum);

	// The directive is in the form of `.byte expr[, expr...]`
	// Each expression will be in the form of its AST, with the directive holding these ASTs
	//     .byte
	// 		 |
	//     +---+---+---+---...
	//     |   |   |   |
	//    expr expr expr <-- these are just sub-ASTs
	//     |	 |  |   |
	//   ... ... ... ...

	Token* nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type == TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.byte` directive must be followed by at least one expression.");

	// This array is for the data table to hold, read the comment in DataTable.h for more info
	Node** byteArray = newNodeArray(2);
	if (!byteArray) emitError(ERR_MEM, NULL, "Failed to allocate memory for `.byte` directive expressions.");
	int byteArrayCapacity = 2;
	int byteArrayCount = 0;
	// The number of bytes that the data will occupy is just the number of expressions (since each expr is 1 byte)
	// So the size is just the count

	// Make the exception that single-value expressions do not need to contain '#'
	if (nextToken->type == TK_INTEGER) nextToken->type = TK_IMM;
	while (true) {
		Node* exprRoot = parseExpression(parser);
		addNaryDirectiveData(directiveData, exprRoot);
		exprRoot->parent = directiveRoot;

		byteArray = nodeArrayInsert(byteArray, &byteArrayCapacity, &byteArrayCount, exprRoot);
		if (!byteArray) emitError(ERR_MEM, NULL, "Failed to reallocate memory for `.byte` directive expressions.");

		// Assume currentTokenIndex was updated in parseExpression for now
		nextToken = parser->tokens[parser->currentTokenIndex]; // This better be the comma or newline
		if (nextToken->type == TK_NEWLINE) {
			// End of the directive
			parser->currentTokenIndex++; // Consume the newline
			break;
		} else if (nextToken->type == TK_COMMA) {
			// More expressions to come
			parser->currentTokenIndex++; // Consume the comma
			nextToken = parser->tokens[parser->currentTokenIndex];
			if (nextToken->type == TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "Trailing comma in `.byte` directive is not allowed.");
			continue;
		} else {
			emitError(ERR_INVALID_SYNTAX, &linedata, "Expected `,` or newline after expression in `.byte` directive, got `%s`.", nextToken->lexeme);
		}
	}

	uint32_t dataAddr = parser->sectionTable->entries[parser->sectionTable->activeSection].lp;
	uint32_t dataSize = byteArrayCount; // Each expr is 1 byte
	// Set the data for the data table
	data_entry_t* bytesDataEntry = initDataEntry(BYTES_TYPE, dataAddr, dataSize, byteArray, byteArrayCount, byteArrayCapacity);
	parser->sectionTable->entries[parser->sectionTable->activeSection].lp += byteArrayCount;
	addDataEntry(parser->dataTable, bytesDataEntry, parser->sectionTable->activeSection);
}

void handleHword(Parser* parser, Node* directiveRoot) {
	// Exact same thing as `handleByte` but with each expression being 2 bytes
	initScope("handleHword");

	Token* directiveToken = parser->tokens[parser->currentTokenIndex++];
	linedata_ctx linedata = {
		.linenum = directiveToken->linenum,
		.source = ssGetString(directiveToken->sstring)
	};

	directiveToken->type = TK_D_HWORD;

	validateSection(parser, directiveToken, &linedata);

	DirctvNode* directiveData = initDirectiveNode();
	setNodeData(directiveRoot, directiveData, ND_DIRECTIVE);

	log("Handling .hword directive at line %d", directiveToken->linenum);

	Token* nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type == TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.hword` directive must be followed by at least one expression.");

	Node** hwordArray = newNodeArray(2);
	if (!hwordArray) emitError(ERR_MEM, NULL, "Failed to allocate memory for `.hword` directive expressions.");
	int hwordArrayCapacity = 2;
	int hwordArrayCount = 0;

	// Make the exception that single-value expressions do not need to contain '#'
	if (nextToken->type == TK_INTEGER) nextToken->type = TK_IMM;
	while (true) {
		Node* exprRoot = parseExpression(parser);
		addNaryDirectiveData(directiveData, exprRoot);
		exprRoot->parent = directiveRoot;

		hwordArray = nodeArrayInsert(hwordArray, &hwordArrayCapacity, &hwordArrayCount, exprRoot);
		if (!hwordArray) emitError(ERR_MEM, NULL, "Failed to reallocate memory for `.hword` directive expressions.");

		nextToken = parser->tokens[parser->currentTokenIndex];
		if (nextToken->type == TK_NEWLINE) {
			parser->currentTokenIndex++;
			break;
		} else if (nextToken->type == TK_COMMA) {
			parser->currentTokenIndex++;
			nextToken = parser->tokens[parser->currentTokenIndex];
			if (nextToken->type == TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "Trailing comma in `.hword` directive is not allowed.");
			continue;
		} else {
			emitError(ERR_INVALID_SYNTAX, &linedata, "Expected `,` or newline after expression in `.hword` directive, got `%s`.", nextToken->lexeme);
		}
	}

	uint32_t dataAddr = parser->sectionTable->entries[parser->sectionTable->activeSection].lp;
	uint32_t dataSize = hwordArrayCount * 2; // Each expr is 2 bytes
	data_entry_t* hwordsDataEntry = initDataEntry(HWORDS_TYPE, dataAddr, dataSize, hwordArray, hwordArrayCount, hwordArrayCapacity);
	parser->sectionTable->entries[parser->sectionTable->activeSection].lp += dataSize;
	addDataEntry(parser->dataTable, hwordsDataEntry, parser->sectionTable->activeSection);
}

void handleWord(Parser* parser, Node* directiveRoot) {
	// Exact same thing as `handleByte` but with each expression being 4 bytes
	initScope("handleWord");

	Token* directiveToken = parser->tokens[parser->currentTokenIndex++];
	linedata_ctx linedata = {
		.linenum = directiveToken->linenum,
		.source = ssGetString(directiveToken->sstring)
	};

	directiveToken->type = TK_D_WORD;

	validateSection(parser, directiveToken, &linedata);

	DirctvNode* directiveData = initDirectiveNode();
	setNodeData(directiveRoot, directiveData, ND_DIRECTIVE);

	log("Handling .word directive at line %d", directiveToken->linenum);

	Token* nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type == TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.word` directive must be followed by at least one expression.");

	Node** wordArray = newNodeArray(2);
	if (!wordArray) emitError(ERR_MEM, NULL, "Failed to allocate memory for `.word` directive expressions.");
	int wordArrayCapacity = 2;
	int wordArrayCount = 0;

	// Make the exception that single-value expressions do not need to contain '#'
	if (nextToken->type == TK_INTEGER) nextToken->type = TK_IMM;
	while (true) {
		Node* exprRoot = parseExpression(parser);
		addNaryDirectiveData(directiveData, exprRoot);
		exprRoot->parent = directiveRoot;

		wordArray = nodeArrayInsert(wordArray, &wordArrayCapacity, &wordArrayCount, exprRoot);
		if (!wordArray) emitError(ERR_MEM, NULL, "Failed to reallocate memory for `.word` directive expressions.");

		nextToken = parser->tokens[parser->currentTokenIndex];
		if (nextToken->type == TK_NEWLINE) {
			parser->currentTokenIndex++;
			break;
		} else if (nextToken->type == TK_COMMA) {
			parser->currentTokenIndex++;
			nextToken = parser->tokens[parser->currentTokenIndex];
			if (nextToken->type == TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "Trailing comma in `.word` directive is not allowed.");
			continue;
		} else {
			emitError(ERR_INVALID_SYNTAX, &linedata, "Expected `,` or newline after expression in `.word` directive, got `%s`.", nextToken->lexeme);
		}
	}

	uint32_t dataAddr = parser->sectionTable->entries[parser->sectionTable->activeSection].lp;
	uint32_t dataSize = wordArrayCount * 4; // Each expr is 4 bytes
	data_entry_t* wordsDataEntry = initDataEntry(WORDS_TYPE, dataAddr, dataSize, wordArray, wordArrayCount, wordArrayCapacity);
	parser->sectionTable->entries[parser->sectionTable->activeSection].lp += dataSize;
	addDataEntry(parser->dataTable, wordsDataEntry, parser->sectionTable->activeSection);
}

void handleFloat(Parser* parser, Node* directiveRoot) {
	initScope("handleFloat");

	Token* directiveToken = parser->tokens[parser->currentTokenIndex++];
	linedata_ctx linedata = {
		.linenum = directiveToken->linenum,
		.source = ssGetString(directiveToken->sstring)
	};

	directiveToken->type = TK_D_FLOAT;

	validateSection(parser, directiveToken, &linedata);

	DirctvNode* directiveData = initDirectiveNode();
	setNodeData(directiveRoot, directiveData, ND_DIRECTIVE);

	log("Handling .float directive at line %d", directiveToken->linenum);

	// The directive is in the form of `.float float[, float...]`
	// So there will be no expression trees, just number nodes
	// These is still the need to have the array for the data table

	Token* nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type == TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.float` directive must be followed by at least one float.");
	if (nextToken->type != TK_FLOAT) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.float` directive must be followed by a float, got `%s`.", nextToken->lexeme);

	Node** floatArray = newNodeArray(2);
	if (!floatArray) emitError(ERR_MEM, NULL, "Failed to allocate memory for `.float` directive floats.");
	int floatArrayCapacity = 2;
	int floatArrayCount = 0;

	while (true) {
		Node* floatNode = initASTNode(AST_LEAF, ND_NUMBER, nextToken, directiveRoot);
		NumNode* numData = initNumberNode(NTYPE_FLOAT, 0, strtof(nextToken->lexeme, NULL));
		addNaryDirectiveData(directiveData, floatNode);
		setNodeData(floatNode, numData, ND_NUMBER);

		floatArray = nodeArrayInsert(floatArray, &floatArrayCapacity, &floatArrayCount, floatNode);
		if (!floatArray) emitError(ERR_MEM, NULL, "Failed to reallocate memory for `.float` directive floats.");

		parser->currentTokenIndex++; // Consume the float token
		nextToken = parser->tokens[parser->currentTokenIndex];
		if (nextToken->type == TK_NEWLINE) {
			parser->currentTokenIndex++; // Consume the newline
			break;
		} else if (nextToken->type != TK_COMMA) {
			emitError(ERR_INVALID_SYNTAX, &linedata, "Expected `,` or newline after float in `.float` directive, got `%s`.", nextToken->lexeme);
		} else {
			// More floats to come
			parser->currentTokenIndex++; // Consume the comma
			nextToken = parser->tokens[parser->currentTokenIndex];
			if (nextToken->type == TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "Trailing comma in `.float` directive is not allowed.");
			if (nextToken->type != TK_FLOAT) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.float` directive must be followed by a float, got `%s`.", nextToken->lexeme);
		}
	}

	uint32_t dataAddr = parser->sectionTable->entries[parser->sectionTable->activeSection].lp;
	uint32_t dataSize = floatArrayCount * 4; // Each float is 4 bytes
	data_entry_t* floatsDataEntry = initDataEntry(FLOATS_TYPE, dataAddr, dataSize, floatArray, floatArrayCount, floatArrayCapacity);
	parser->sectionTable->entries[parser->sectionTable->activeSection].lp += dataSize;
	addDataEntry(parser->dataTable, floatsDataEntry, parser->sectionTable->activeSection);
}

void handleZero(Parser* parser, Node* directiveRoot) {
	initScope("handleZero");

	Token* directiveToken = parser->tokens[parser->currentTokenIndex++];
	linedata_ctx linedata = {
		.linenum = directiveToken->linenum,
		.source = ssGetString(directiveToken->sstring)
	};

	directiveToken->type = TK_D_ZERO;

	validateSection(parser, directiveToken, &linedata);

	DirctvNode* directiveData = initDirectiveNode();
	setNodeData(directiveRoot, directiveData, ND_DIRECTIVE);

	log("Handling .zero directive at line %d", directiveToken->linenum);

	// The directive is in the form of `.zero size`
	// size is allowed to be an expression
	// The expression will be in the form of its AST, with the directive holding this AST
	//     .zero
	//       |
	//      expr
	//       |
	//     ... ...

	Token* nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type == TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.zero` directive must be followed by an expression.");

	Node* exprRoot = parseExpression(parser);
	// setUnaryDirectiveData(directiveData, exprRoot);
	addNaryDirectiveData(directiveData, exprRoot);
	exprRoot->parent = directiveRoot;

	nextToken = parser->tokens[parser->currentTokenIndex]; // This better be the newline
	if (nextToken->type != TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.zero` directive must be followed by only an expression on the same line.");
	parser->currentTokenIndex++; // Consume the newline

	uint32_t dataAddr = parser->sectionTable->entries[parser->sectionTable->activeSection].lp;
	// The size of the zeroed data is determined by evaluating the expression
	int exprEvalResult = evaluateExpression(exprRoot, parser->symbolTable);
	if (exprEvalResult < 0) emitError(ERR_INVALID_EXPRESSION, &linedata, "Failed to evaluate expression in `.zero` directive.");
	uint32_t dataSize = (uint32_t)exprEvalResult;
	parser->sectionTable->entries[parser->sectionTable->activeSection].lp += dataSize;

	// Set the data for the data table
	data_entry_t* zeroDataEntry = initDataEntry(BYTES_TYPE, dataAddr, dataSize, directiveData->nary.exprs, 1, directiveData->nary.exprCapacity);
	addDataEntry(parser->dataTable, zeroDataEntry, parser->sectionTable->activeSection);
}

void handleFill(Parser* parser, Node* directiveRoot) {
	emitWarning(WARN_UNIMPLEMENTED, NULL, "The `.fill` directive is not yet implemented.");
}


void handleSize(Parser* parser, Node* directiveRoot) {
	emitWarning(WARN_UNIMPLEMENTED, NULL, "The `.size` directive is not yet implemented.");
}

void handleType(Parser* parser, Node* directiveRoot) {
	initScope("handleType");

	Token* directiveToken = parser->tokens[parser->currentTokenIndex++];
	linedata_ctx linedata = {
		.linenum = directiveToken->linenum,
		.source = ssGetString(directiveToken->sstring)
	};

	// Disallow when types feature is disabled
	if (!FEATURE_ENABLED(parser->config, FEATURE_TYPES)) emitError(ERR_UNSUPPORTED, &linedata, "The `.type` directive is not supported because types feature is disabled.");

	directiveToken->type = TK_D_TYPE;

	DirctvNode* directiveData = initDirectiveNode();
	setNodeData(directiveRoot, directiveData, ND_DIRECTIVE);

	log("Handling .type directive at line %d", directiveToken->linenum);

	// The directive is in the form of `.type symbol, $[MainType]{.[SubType]{.[Tag]}}`

	Token* nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type == TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.type` directive must be followed by a symbol and a type.");
	if (nextToken->type != TK_IDENTIFIER) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.type` directive must be followed by an identifier, got `%s`.", nextToken->lexeme);
	// Since the lexer bunched up many things under TK_IDENTIFIER, it needs to be checked whether the token is actually a symbol
	// What constitutes a symbol is the same as for a label, just reuse the logic
	validateSymbolToken(nextToken, &linedata);
	Token* symbToken = nextToken;

	Node* symbNode = initASTNode(AST_LEAF, ND_SYMB, symbToken, directiveRoot);
	SymbNode* symbData = initSymbolNode(-1, 0); // -1 for now, will update later
	setNodeData(symbNode, symbData, ND_SYMB);
	// Not setting the directive children until the data type is acquired

	symb_entry_t* symbEntry = getSymbolEntry(parser->symbolTable, symbToken->lexeme);
	if (!symbEntry) {
		// Since .type only sets the type status, and this is its first sighting, not much is known
		// Just go with defaults/assumptions that it is absolute, no type, and value
		// Also, since this is just a directive, there will be no activeSection info
		SYMBFLAGS flags = CREATE_FLAGS(M_ABS, T_NONE, E_VAL, S_UNDEF, L_LOC, R_NREF, D_UNDEF);
		symbEntry = initSymbolEntry(symbToken->lexeme, flags, NULL, 0, NULL, -1);
		addSymbolEntry(parser->symbolTable, symbEntry);
	}
	symbEntry->structTypeIdx = -1; // This might be changed later on encountering the tag token

	parser->currentTokenIndex++; // Consume the symbol token
	// Make sure comma is next
	nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type != TK_COMMA) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.type` directive must have a comma after the symbol.");
	parser->currentTokenIndex++; // Consume the comma

	// Onwards to the main type
	nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type != TK_MAIN_TYPE) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.type` directive must be followed by a main type, got `%s`.", nextToken->lexeme);

	// Make sure the main type is a valid main type
	TypeNode* mainTypeData = initTypeNode();
	if (strcasecmp(nextToken->lexeme, "$function") == 0) mainTypeData->mainType = TYPE_FUNC;
	else if (strcasecmp(nextToken->lexeme, "$object") == 0) mainTypeData->mainType = TYPE_OBJECT;
	else emitError(ERR_INVALID_TYPE, &linedata, "Invalid main type in `.type` directive: `%s`.", nextToken->lexeme);
	Token* mainTypeToken = nextToken;

	Node* mainTypeNode = initASTNode(AST_INTERNAL, ND_TYPE, mainTypeToken, directiveRoot);
	setNodeData(mainTypeNode, mainTypeData, ND_TYPE);
	setBinaryDirectiveData(directiveData, symbNode, mainTypeNode); // Connect symb and main type to directive

	// Update the symbol's type
	// Because of the value of M_* differs than that of TYPE_* by 2, need to add 2
	// M_FUNC is 2 while TYPE_FUNC is 0
	// M_OBJECT is 3 while TYPE_OBJECT is 1
	symbEntry->flags = SET_MAIN_TYPE(symbEntry->flags, (mainTypeData->mainType + 2));

	parser->currentTokenIndex++; // Consume the main type token


	// Check for optional sub-type by detecting a dot
	nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type == TK_NEWLINE) {
		// Since main type is the end, mark it as leaf
		mainTypeNode->astNodeType = AST_LEAF;
		return;
	}
	if (nextToken->type != TK_DOT) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected newline or `.` after main type in `.type` directive, got `%s`.", nextToken->lexeme);
	parser->currentTokenIndex++; // Consume the dot

	nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type != TK_IDENTIFIER) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.type` directive must be followed by a sub-type after the dot, got `%s`.", nextToken->lexeme);
	// Since the lexer bunched many things under TK_IDENTIFIER, need to check if it is
	// Make sure the sub-type is a valid sub-type
	TypeNode* subTypeData = initTypeNode();
	if (strcasecmp(nextToken->lexeme, "array") == 0) subTypeData->subType = TYPE_ARRAY;
	else if (strcasecmp(nextToken->lexeme, "ptr") == 0) subTypeData->subType = TYPE_PTR;
	else if (strcasecmp(nextToken->lexeme, "struct") == 0) subTypeData->subType = TYPE_STRUCT;
	else if (strcasecmp(nextToken->lexeme, "union") == 0) subTypeData->subType = TYPE_UNION;
	else emitError(ERR_INVALID_TYPE, &linedata, "Invalid sub-type in `.type` directive: `%s`.", nextToken->lexeme);
	Token* subTypeToken = nextToken;
	subTypeToken->type = TK_SUB_TYPE;

	Node* subTypeNode = initASTNode(AST_INTERNAL, ND_TYPE, subTypeToken, mainTypeNode);
	setNodeData(subTypeNode, subTypeData, ND_TYPE);
	setUnaryTypeData(mainTypeData, subTypeNode);

	// Update the symbol's type
	// Because of the value of T_* differs than that of TYPE_* by 1, need to add 1
	// T_NONE is 0 while TYPE_NONE is 0
	// T_ARRAY is 1 while TYPE_ARRAY is 1
	// T_PTR is 2 while TYPE_PTR is 2
	// T_STRUCT is 3 while TYPE_STRUCT is 3
	// T_UNION is 4 while TYPE_UNION is 4
	symbEntry->flags = SET_SUB_TYPE(symbEntry->flags, (subTypeData->subType - 1));

	parser->currentTokenIndex++; // Consume the sub-type token


	// Check for optional tag by detecting a dot
	nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type == TK_NEWLINE) {
		// Since subtype type is the end, mark it as leaf
		subTypeNode->astNodeType = AST_LEAF;
		return;
	}
	if (nextToken->type != TK_DOT) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected newline or `.` after sub type in `.type` directive, got `%s`.", nextToken->lexeme);
	parser->currentTokenIndex++;

	nextToken = parser->tokens[parser->currentTokenIndex++];
	if (nextToken->type != TK_IDENTIFIER) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.type` directive must be followed by a tag after the dot, got `%s`.", nextToken->lexeme);


	// Recall that the lexer bunched many things under TK_IDENTIFIER, so need to check if it is a valid tag
	// Valid tag as in same thing as a symbol (after all, the tags are just symbol names)
	validateSymbolToken(nextToken, &linedata);
	Token* tagToken = nextToken;
	tagToken->type = TK_SUB_TYPE; // Maybe have the tag as identifier type???

	// Optional check, use the symbol table to see if the user used a symbol rather than a struct

	// .def declarations must be declared before use, check if it exists
	struct_root_t* structRoot = getStructByName(parser->structTable, tagToken->lexeme);
	if (!structRoot) emitError(ERR_UNDEFINED, &linedata, "Tag in `.type` directive is not defined: `%s`.", tagToken->lexeme);

	TypeNode* tagTypeData = initTypeNode();
	tagTypeData->structTableIndex = structRoot->index;
	Node* tagNode = initASTNode(AST_LEAF, ND_SYMB, tagToken, subTypeNode);
	setNodeData(tagNode, tagTypeData, ND_TYPE);
	setUnaryTypeData(subTypeData, tagNode);
	
	// Thinking about it, should the tag be allowed for non-defs? For example, `.type arr, $object.array.byte` where byte is not a def
	//  and it means an array of bytes???? Need to rework it if so
	// For now, it is only structs/union

	// Update the symbol's struct type index
	symbEntry->structTypeIdx = structRoot->index;
	// Since the struct is know, it can inherit the size
	symbEntry->size = structRoot->size;
}


void handleAlign(Parser* parser, Node* directiveRoot) {
	emitWarning(WARN_UNIMPLEMENTED, NULL, "The `.align` directive is not yet implemented.");
}

void handleExtern(Parser* parser, Node* directiveRoot) {
	emitWarning(WARN_UNIMPLEMENTED, NULL, "The `.extern` directive is not yet implemented.");
	// This will require the need for relocation tables
}

void handleInclude(Parser* parser) {
	initScope("handleInclude");

	Token* directiveToken = parser->tokens[parser->currentTokenIndex++];
	linedata_ctx linedata = {
		.linenum = directiveToken->linenum,
		.source = ssGetString(directiveToken->sstring)
	};

	log("Handling .include directive at line %d", directiveToken->linenum);

	// The directive is in the form of `.include "filename"`

	Token* nextToken = parser->tokens[parser->currentTokenIndex++];
	if (nextToken->type == TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.include` directive must be followed by a filename.");
	if (nextToken->type != TK_STRING) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.include` directive must be followed by a string, got `%s`.", nextToken->lexeme);
	sds filename = sdsnewlen(nextToken->lexeme + 1, sdslen(nextToken->lexeme) - 2);
	if (!filename) emitError(ERR_MEM, NULL, "Failed to allocate memory for `.include` directive filename.");

	// Leave the rest to the adecl module thing
	FILE* adeclFile = openADECLFile(filename);
	if (!adeclFile) emitError(ERR_IO, &linedata, "Failed to open file `%s` for `.include` directive.", filename);
	
	ADECL_ctx context = {
		.parentParserConfig = parser->config,
		.symbolTable = parser->symbolTable,
		.structTable = parser->structTable,
		.asts = NULL,
		.astCapacity = 0,
		.astCount = 0
	};
	lexParseADECLFile(adeclFile, &context);

	// Now, merge the ASTs, symbol table, and struct table

	for (int i = 0; i < context.astCount; i++) {
		parser->asts = nodeArrayInsert(parser->asts, &parser->astCapacity, &parser->astCount, context.asts[i]);
		if (!parser->asts) emitError(ERR_MEM, NULL, "Failed to reallocate memory for ASTs after processing `.include` directive.");
	}

	for (int i = 0; i < context.symbolTable->size; i++) {
		symb_entry_t* entry = context.symbolTable->entries[i];
		symb_entry_t* existingEntry = getSymbolEntry(parser->symbolTable, entry->name);
		if (existingEntry) {
			// If the existing entry is defined and the new one is also defined, error
			if (GET_DEFINED(existingEntry->flags) && GET_DEFINED(entry->flags)) {
				emitError(ERR_REDEFINED, &linedata, "Symbol redefinition from `.include` directive: `%s`. First defined at `%s`", entry->name, ssGetString(existingEntry->source));
			}
			// Otherwise, merge the entries
			// If the new entry is defined, update the existing one to be defined
			if (GET_DEFINED(entry->flags)) {
				SET_DEFINED(existingEntry->flags);
				existingEntry->value = entry->value;
				existingEntry->linenum = entry->linenum;
				existingEntry->source = entry->source;
			}
			// If the new entry is global, update the existing one to be global
			if (GET_LOCALITY(entry->flags) == L_GLOB) {
				SET_LOCALITY(existingEntry->flags);
			}
			// Merge references
			for (int j = 0; j < entry->references.refcount; j++) {
				addSymbolReference(existingEntry, entry->references.refs[j]->source, entry->references.refs[j]->linenum);
			}
		} else {
			// Just add the new entry as is
			symb_entry_t* newEntry = initSymbolEntry(entry->name, entry->flags, entry->value.expr, entry->value.val, entry->source, entry->linenum);
			for (int j = 0; j < entry->references.refcount; j++) {
				addSymbolReference(newEntry, entry->references.refs[j]->source, entry->references.refs[j]->linenum);
			}
			addSymbolEntry(parser->symbolTable, newEntry);
		}
	}
	// Safe to free the table????
	deinitSymbolTable(context.symbolTable);

	for (int i = 0; i < context.structTable->size; i++) {
		struct_root_t* structRoot = context.structTable->structs[i];
		struct_root_t* existingStruct = getStructByName(parser->structTable, structRoot->name);
		if (existingStruct) {
			emitError(ERR_REDEFINED, &linedata, "Struct redefinition from `.include` directive: `%s`. First defined at `%s`", structRoot->name, ssGetString(existingStruct->source));
		} else {
			// Just add the new struct as is
			struct_root_t* newStruct = initStruct(structRoot->name);
			for (int j = 0; j < structRoot->fieldCount; j++) {
				struct_field_t* field = structRoot->fields[j];
				struct_field_t* newField = initStructField(field->name, field->type, field->size, field->offset, field->structTypeIdx);
				newField->source = field->source;
				newField->linenum = field->linenum;
				addStructField(newStruct, newField);
			}
			newStruct->size = structRoot->size;
			newStruct->source = structRoot->source;
			newStruct->linenum = structRoot->linenum;
			addStruct(parser->structTable, newStruct);
		}
		
	}
	deinitStructTable(context.structTable);

	free(context.asts);
}

void handleDef(Parser* parser, Node* directiveRoot) {
	initScope("handleDef");

	Token* directiveToken = parser->tokens[parser->currentTokenIndex++];
	linedata_ctx linedata = {
		.linenum = directiveToken->linenum,
		.source = ssGetString(directiveToken->sstring)
	};

	// Disallow when types feature is disabled
	if (!FEATURE_ENABLED(parser->config, FEATURE_TYPES)) emitError(ERR_UNSUPPORTED, &linedata, "The `.def` directive is not supported because types feature is disabled.");

	directiveToken->type = TK_D_DEF;

	DirctvNode* directiveData = initDirectiveNode();
	setNodeData(directiveRoot, directiveData, ND_DIRECTIVE);

	log("Handling .def directive at line %d", directiveToken->linenum);

	// The directive is in the form of `.def StructName { field1: type1. field2:: type2. ... }`

	Token* nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type == TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.def` directive must be followed by a struct name.");
	if (nextToken->type != TK_IDENTIFIER) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.def` directive must be followed by an identifier, got `%s`.", nextToken->lexeme);
	// Since the lexer bunched up many things under TK_IDENTIFIER, need to check if it is actually a symbol
	// What constitutes a symbol is the same as for a label, just reuse the logic
	validateSymbolToken(nextToken, &linedata);
	Token* structNameToken = nextToken;

	struct_root_t* defStruct = getStructByName(parser->structTable, structNameToken->lexeme);
	// Make sure it has not be defined
	if (defStruct) emitError(ERR_REDEFINED, &linedata, "Struct redefinition: `%s`. First defined at `%s`", structNameToken->lexeme, ssGetString(defStruct->source));
	defStruct = initStruct(structNameToken->lexeme);
	defStruct->source = structNameToken->sstring;
	defStruct->linenum = structNameToken->linenum;

	parser->currentTokenIndex++; // Consume the struct name token

	nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type != TK_LBRACKET) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected `{`, got `%s`.", nextToken->lexeme);
	parser->currentTokenIndex++; // Consume the '{' token

	nextToken = parser->tokens[parser->currentTokenIndex];
	// Now, the next can either be newlines or the field themselves
	while (nextToken->type == TK_NEWLINE) {
		parser->currentTokenIndex++; // Consume the newline
		nextToken = parser->tokens[parser->currentTokenIndex];
	}
	linedata.linenum = nextToken->linenum;
	linedata.source = ssGetString(nextToken->sstring);

	// Now to loop "forever" to get the fields
	// However, need some way to stop. What if `}` is missing
	// Actually, I think this is where the EOF token really comes in handy
	// But as mentioned in lexer, the EOF gets the same type as a newline but keeps the lexeme as "EOF"
	// Kind of costly but in the check for newline, can also check if lexeme is "EOF"

	while (nextToken->type != TK_RBRACKET) {
		// Each field is in the form of `fieldName: basic-type.` or `fieldName:: def-type.`

		if (nextToken->type == TK_NEWLINE) {
			if (sdscmp(nextToken->lexeme, "EOF") == 0) emitError(ERR_INVALID_SYNTAX, &linedata, "Unexpected end of file while parsing `.def` directive for struct `%s`.", structNameToken->lexeme);

			parser->currentTokenIndex++;
			nextToken = parser->tokens[parser->currentTokenIndex];

			// Update linedata
			linedata.linenum = nextToken->linenum;
			linedata.source = ssGetString(nextToken->sstring);
			continue;
		}


		if (nextToken->type != TK_IDENTIFIER) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected field name identifier, got `%s`.", nextToken->lexeme);
		validateSymbolToken(nextToken, &linedata);
		Token* fieldNameToken = nextToken;

		parser->currentTokenIndex++; // Consume the field name token

		nextToken = parser->tokens[parser->currentTokenIndex];
		if (nextToken->type != TK_COLON && nextToken->type != TK_COLON_COLON) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected `:` or `::` after field name, got `%s`.", nextToken->lexeme);

		parser->currentTokenIndex++; // Consume the colon/colon-colon token

		bool isNumericType = (nextToken->type == TK_COLON) ? true : false;

		nextToken = parser->tokens[parser->currentTokenIndex];
		if (nextToken->type == TK_INTEGER && parser->tokens[parser->currentTokenIndex-1]->type == TK_COLON_COLON) {
			emitError(ERR_INVALID_SYNTAX, &linedata, "Expected defined type but got integer `%s`. Did you mean to use `:` instead of `::`?", nextToken->lexeme);
		} else if (nextToken->type == TK_IDENTIFIER && parser->tokens[parser->currentTokenIndex-1]->type == TK_COLON) {
			emitError(ERR_INVALID_SYNTAX, &linedata, "Expected basic type but got identifier `%s`. Did you mean to use `::` instead of `:`?", nextToken->lexeme);
		}
		if (nextToken->type != TK_INTEGER && nextToken->type != TK_IDENTIFIER) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected numeric type after `:` or def type after `::`, got `%s`.", nextToken->lexeme);

		structFieldType fieldType = BYTE_FT;
		int size = 0;
		int structTypeIdx = -1; // Only used if fieldType is STRUCT_FT

		// Check the types are appropriate, per type
		if (isNumericType) {
			// Updated field type and size
			if (*nextToken->lexeme == '8') {
				fieldType = BYTE_FT;
				size = 1;
			} else if (*nextToken->lexeme == '1' && *(nextToken->lexeme + 1) == '6') {
				fieldType = HWORD_FT;
				size = 2;
			} else if (*nextToken->lexeme == '3' && *(nextToken->lexeme + 1) == '2') {
				fieldType = WORD_FT;
				size = 4;
			} else {
				emitError(ERR_INVALID_TYPE, &linedata, "Invalid basic type for struct field: `%s`. Only 8, 16, and 32 are allowed.", nextToken->lexeme);
			}
		} else {
			// In the case that the type is itself, disallow
			if (sdscmp(nextToken->lexeme, structNameToken->lexeme) == 0) {
				emitError(ERR_INVALID_TYPE, &linedata, "Struct field `%s` in struct `%s` cannot be of the same struct type. Consider using a pointer type.", 
						fieldNameToken->lexeme, structNameToken->lexeme);
			} else {
				// Defined type, so check if it exists
				struct_root_t* fieldDefStruct = getStructByName(parser->structTable, nextToken->lexeme);
				if (!fieldDefStruct) emitError(ERR_UNDEFINED, &linedata, "Undefined struct type for struct field: `%s`.", nextToken->lexeme);
				fieldType = STRUCT_FT;
				size = fieldDefStruct->size;
				structTypeIdx = fieldDefStruct->index;
			}
		}

		struct_field_t* newField = initStructField(fieldNameToken->lexeme, fieldType, size, defStruct->size, structTypeIdx);
		newField->source = fieldNameToken->sstring;
		newField->linenum = fieldNameToken->linenum;
		bool added = addStructField(defStruct, newField);
		if (!added) emitError(ERR_REDEFINED, &linedata, "Redefinition of field `%s` in struct `%s`.", fieldNameToken->lexeme, structNameToken->lexeme);

		// Finished with this field, next token needs to be the dot
		parser->currentTokenIndex++; // Consume the type token
		nextToken = parser->tokens[parser->currentTokenIndex];
		if (nextToken->type != TK_DOT) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected `.` after struct field type, got `%s`.", nextToken->lexeme);
		parser->currentTokenIndex++; // Consume the dot token
		nextToken = parser->tokens[parser->currentTokenIndex];
	}

	// Finished all fields, nextToken is '}'
	parser->currentTokenIndex++; // Consume the '}' token

	addStruct(parser->structTable, defStruct);
}

void handleSizeof(Parser* parser, Node* directiveRoot) {
	emitWarning(WARN_UNIMPLEMENTED, NULL, "The `.sizeof` directive is not yet implemented.");
}