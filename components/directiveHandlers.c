#include <ctype.h>

#include "handlers.h"
#include "diagnostics.h"
#include "expr.h"
#include "reserved.h"


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


void handleString(Parser* parser, Node* directiveRoot) {
	initScope("handleString");

	Token* directiveToken = parser->tokens[parser->currentTokenIndex++];
	linedata_ctx linedata = {
		.linenum = directiveToken->linenum,
		.source = ssGetString(directiveToken->sstring)
	};

	directiveToken->type = TK_D_STRING;

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

	DirctvNode* directiveData = initDirectiveNode();
	setNodeData(directiveRoot, directiveData, ND_DIRECTIVE);

	log("Handling .hword directive at line %d", directiveToken->linenum);

	Token* nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type == TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.hword` directive must be followed by at least one expression.");

	Node** hwordArray = newNodeArray(2);
	if (!hwordArray) emitError(ERR_MEM, NULL, "Failed to allocate memory for `.hword` directive expressions.");
	int hwordArrayCapacity = 2;
	int hwordArrayCount = 0;

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

	DirctvNode* directiveData = initDirectiveNode();
	setNodeData(directiveRoot, directiveData, ND_DIRECTIVE);

	log("Handling .word directive at line %d", directiveToken->linenum);

	Token* nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type == TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "The `.word` directive must be followed by at least one expression.");

	Node** wordArray = newNodeArray(2);
	if (!wordArray) emitError(ERR_MEM, NULL, "Failed to allocate memory for `.word` directive expressions.");
	int wordArrayCapacity = 2;
	int wordArrayCount = 0;

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

void handleFloat(Parser *parser, Node *directiveRoot)
{
}

void handleZero(Parser *parser, Node *directiveRoot)
{
}

void handleFill(Parser *parser, Node *directiveRoot)
{
}

void handleSize(Parser *parser, Node *directiveRoot)
{
}

void handleType(Parser *parser, Node *directiveRoot)
{
}

void handleAlign(Parser *parser, Node *directiveRoot)
{
}

void handleExtern(Parser *parser, Node *directiveRoot)
{
}

void handleInclude(Parser *parser, Node *directiveRoot)
{
}

void handleDef(Parser *parser, Node *directiveRoot)
{
}

void handleSizeof(Parser *parser, Node *directiveRoot)
{
}