#include "handlers.h"
#include "diagnostics.h"
#include "expr.h"


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

	while (true) {
		Node* exprRoot = parseExpression(parser);
		addNaryDirectiveData(directiveData, exprRoot);
		exprRoot->parent = directiveRoot;

		// Assume currentTokenIndex was updated in parseExpression for now
		nextToken = parser->tokens[parser->currentTokenIndex]; // This better be the comma or newline
		if (nextToken->type == TK_NEWLINE) {
			// End of the directive
			parser->currentTokenIndex++; // Consume the newline
			// The parser loops takes care of the newline
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
}