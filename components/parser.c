#include <stdlib.h>
#include <ctype.h>

#include "parser.h"
#include "diagnostics.h"
#include "reserved.h"
#include "handlers.h"


Parser* initParser(Token** tokens, int tokenCount, ParserConfig config) {
	Parser* parser = (Parser*) malloc(sizeof(Parser));
	if (!parser) emitError(ERR_MEM, NULL, "Failed to allocate memory for parser.");

	parser->tokens = tokens;
	parser->tokenCount = tokenCount;
	parser->currentTokenIndex = 0;

	parser->asts = (Node**) malloc(sizeof(Node*) * 4);
	if (!parser->asts) emitError(ERR_MEM, NULL, "Failed to allocate memory for parser ASTs.");

	parser->astCount = 0;
	parser->astCapacity = 4;

	parser->config = config;

	return parser;
}

void setTables(Parser* parser, SectionTable* sectionTable, SymbolTable* symbolTable, StructTable* structTable, DataTable* dataTable) {
	parser->sectionTable = sectionTable;
	parser->symbolTable = symbolTable;
	parser->structTable = structTable;
	parser->dataTable = dataTable;
}

void deinitParser(Parser* parser) {
	for (int i = 0; i < parser->astCount; i++) {
		freeAST(parser->asts[i]);
	}
	free(parser->asts);

	free(parser);
}

static void addAst(Parser* parser, Node* ast) {
	if (parser->astCount == parser->astCapacity) {
		parser->astCapacity += 2;
		Node** temp = (Node**) realloc(parser->asts, sizeof(Node*) * parser->astCapacity);
		if (!temp) emitError(ERR_MEM, NULL, "Failed to reallocate memory for parser ASTs.");
		parser->asts = temp;
	}
	parser->asts[parser->astCount++] = ast;
}


static void parseLabel(Parser* parser) {
	Token* labelToken = parser->tokens[parser->currentTokenIndex++];

	// Make sure the label is valid
	if (labelToken->lexeme[0] != '_' && !isalpha(labelToken->lexeme[0])) {
		linedata_ctx linedata = {
			.linenum = labelToken->linenum,
			.source = ssGetString(labelToken->sstring)
		};
		emitError(ERR_INVALID_LABEL, &linedata, "Label must start with an alphabetic character or underscore: `%s`", labelToken->lexeme);
	}
	
	// Now, because of how the lexer works, the lexeme should have stopped at a non-alphanumeric character/underscore
	// This shall be the big assumption here
	// So next is just making sure the label does not use a reserved word	
	// Need to compare against REGISTERS, DIRECTIVES, and INSTRUCTIONS array without caring for case
	if (indexOf(REGISTERS, sizeof(REGISTERS)/sizeof(REGISTERS[0]), labelToken->lexeme) != -1 ||
			indexOf(DIRECTIVES, sizeof(DIRECTIVES)/sizeof(DIRECTIVES[0]), labelToken->lexeme) != -1 ||
			indexOf(INSTRUCTIONS, sizeof(INSTRUCTIONS)/sizeof(INSTRUCTIONS[0]), labelToken->lexeme) != -1) {
		linedata_ctx linedata = {
			.linenum = labelToken->linenum,
			.source = ssGetString(labelToken->sstring)
		};
		emitError(ERR_INVALID_LABEL, &linedata, "Label cannot be a reserved word: `%s`", labelToken->lexeme);
	}

	// Ensure that the symbol has not been defined
	// Note that this means an entry can exist but it is only in the case that it has been referenced before
	// If it has been referenced, just updated the defined status

	symb_entry_t* existingEntry = getSymbolEntry(parser->symbolTable, labelToken->lexeme);
	if (existingEntry && GET_DEFINED(existingEntry->flags)) {
		linedata_ctx linedata = {
			.linenum = labelToken->linenum,
			.source = ssGetString(labelToken->sstring)
		};
		emitError(ERR_REDEFINED, &linedata, "Symbol redefinition: `%s`. First defined at `%s`", labelToken->lexeme, ssGetString(existingEntry->source));
	} else if (existingEntry) {
		// Update the existing entry to be defined now
		SET_DEFINED(existingEntry->flags);
		existingEntry->linenum = labelToken->linenum;
		existingEntry->source = labelToken->sstring; // Maybe have the entry use its own copy of SString
		existingEntry->value.val = parser->sectionTable->entries[parser->sectionTable->activeSection].lp;
	} else {
		SYMBFLAGS flags = CREATE_FLAGS(M_ABS, T_NONE, E_VAL, parser->sectionTable->activeSection, L_LOC, R_NREF, D_DEF);
		uint32_t addr = parser->sectionTable->entries[parser->sectionTable->activeSection].lp;
		symb_entry_t* symbEntry = initSymbolEntry(labelToken->lexeme, flags, NULL, addr, labelToken->sstring, labelToken->linenum);
		if (!symbEntry) emitError(ERR_MEM, NULL, "Failed to create symbol table entry for label.");

		// Add the symbol entry to the symbol table
		addSymbolEntry(parser->symbolTable, symbEntry);
	}
}

static void parseIdentifier(Parser* parser) {
	initScope("parseIdentifier");

	emitWarning(WARN_UNIMPLEMENTED, NULL, "Identifier parsing not yet implemented.");
	parser->currentTokenIndex++;
	return;
}

static void parseDirective(Parser* parser) {
	initScope("parseDirective");

	Token* directiveToken = parser->tokens[parser->currentTokenIndex];

	Node* directiveRoot = initASTNode(AST_ROOT, ND_DIRECTIVE, directiveToken, NULL);
	if (!directiveRoot) emitError(ERR_MEM, NULL, "Failed to create AST node for directive.");

	linedata_ctx linedata = {
		.linenum = directiveToken->linenum,
		.source = ssGetString(directiveToken->sstring)
	};

	// Since the lexer bunched all directives as TK_DIRECTIVE, the actual directive needs to be determined
	// The token field will also be updated to reflect the actual directive

	int index = indexOf(DIRECTIVES, sizeof(DIRECTIVES)/sizeof(DIRECTIVES[0]), directiveToken->lexeme+1);

	if (index == -1) emitError(ERR_INVALID_DIRECTIVE, &linedata, "Unknown directive: `%s`", directiveToken->lexeme+1);

	// To properly identify the directive
	enum Directives directive = (enum Directives) index;

	log("Parsing directive: `%s`. Set type to `%s`", directiveToken->lexeme, DIRECTIVES[directive]);

	// The actions depend on the specific directive

	switch (directive) {
		case DATA: handleData(parser); break;
		case CONST: handleConst(parser); break;
		case BSS: handleBss(parser); break;
		case TEXT: handleText(parser); break;
		case EVT: handleEvt(parser); break;
		case IVT: handleIvt(parser); break;
	
		case SET: handleSet(parser, directiveRoot); break;
		case GLOB: handleGlob(parser, directiveRoot); break;
		case END: // Find a way to stop parsing
			parser->currentTokenIndex++;
			emitWarning(WARN_UNIMPLEMENTED, &linedata, "Directive `%s` not yet implemented!", directiveToken->lexeme);
			break;

		case STRING: handleString(parser, directiveRoot); break;
		case BYTE: handleByte(parser, directiveRoot); break;
		case HWORD: handleHword(parser, directiveRoot); break;
		case WORD: handleWord(parser, directiveRoot); break;
		case FLOAT: handleFloat(parser, directiveRoot); break;
		case ZERO:
		case FILL:

		case ALIGN:
		case SIZE:
		case EXTERN:
		case TYPE: handleType(parser, directiveRoot); break;
		case SIZEOF:
		case DEF: handleDef(parser, directiveRoot); break;
		case INCLUDE: handleInclude(parser); break;
		case TYPEINFO:
			parser->currentTokenIndex++;
			emitWarning(WARN_UNIMPLEMENTED, &linedata, "Directive `%s` not yet implemented!", directiveToken->lexeme);
			break;
		default:
			emitError(ERR_INVALID_DIRECTIVE, &linedata, "Unknown directive: `%s`", directiveToken->lexeme);
			break;
	}

	addAst(parser, directiveRoot);
}


void parse(Parser* parser) {
	initScope("parse");

	int currentTokenIndex = 0;

	while (currentTokenIndex < parser->tokenCount) {
		Token* token = parser->tokens[currentTokenIndex];
		parser->currentTokenIndex = currentTokenIndex;

		switch (token->type) {
			case TK_LABEL:
				parseLabel(parser);
				break;
			case TK_IDENTIFIER:
				// Recall that the lexer bunched many things as TK_IDENTIFIER
				// This is where we determine the actual type
				parseIdentifier(parser);
				break;
			case TK_DIRECTIVE:
				// Similar to TK_IDENTIFIER, the lexer just bunched all of them under TK_DIRECTIVE
				// This is where the specific directive is determined
				parseDirective(parser);
				break;
			case TK_MACRO:
			case TK_REGISTER:
			case TK_IMM:
			case TK_COMMA:
			case TK_LPAREN:
			case TK_RPAREN:
			case TK_LSQBRACKET:
			case TK_RSQBRACKET:
			case TK_LBRACKET:
			case TK_RBRACKET:
			case TK_COLON:
			case TK_COLON_COLON:
			case TK_STRING:
			case TK_DOT:
			case TK_PLUS:
			case TK_MINUS:
			case TK_ASTERISK:
			case TK_DIVIDE:
			case TK_LD_IMM:
			case TK_BITWISE_AND:
			case TK_BITWISE_OR:
			case TK_BITWISE_XOR:
			case TK_BITWISE_NOT:
			case TK_LP:
			case TK_MACRO_ARG:
			case TK_INTEGER:
			case TK_FLOAT:
			case TK_CHAR:
			case TK_OUT:
			case TK_IF:
			case TK_MAIN_TYPE:
			case TK_SUB_TYPE:
			// All of these token types typically follow a directive, an identifier, or a label
			// So these are to be parsed in their respective contexts
			// Encountering them at the top level is an issue
			linedata_ctx linedata = {
				.linenum = token->linenum,
				.source = ssGetString(token->sstring)
			};
			emitError(ERR_INVALID_SYNTAX, &linedata, "Unexpected token: `%s`", token->lexeme);
			break;
			case TK_NEWLINE:
			// Newlines can either appear at the end of a statement or on their own line
			// The end-of-statement newlines are handled in the respective handlers
			// So newlines that appear here are just standalone newlines
			// Ignore, but make sure to advance
			parser->currentTokenIndex++;
			break;
			default: emitError(ERR_INTERNAL, NULL, "Parser encountered unhandled token type: %s", token->lexeme);
		}
		currentTokenIndex = parser->currentTokenIndex;
	}

}