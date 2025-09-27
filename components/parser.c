#include <stdlib.h>

#include "parser.h"
#include "diagnostics.h"
#include "reserved.h"


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

void setTables(Parser* parser, SectionTable* sectionTable, SymbolTable* symbolTable, StructTable* structTable) {
	parser->sectionTable = sectionTable;
	parser->symbolTable = symbolTable;
	parser->structTable = structTable;
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
		if (temp) emitError(ERR_MEM, NULL, "Failed to reallocate memory for parser ASTs.");
		parser->asts = temp;
	}
	parser->asts[parser->astCount++] = ast;
}


static void parseLabel(Parser* parser) {
	Token* labelToken = parser->tokens[parser->currentTokenIndex];


}

static void parseIdentifier(Parser* parser) {
	
}

static void parseDirective(Parser* parser) {
	Token* directiveToken = parser->tokens[parser->currentTokenIndex];

	Node* directiveRoot = initASTNode(AST_ROOT, ND_DIRECTIVE, directiveToken, NULL);
	if (!directiveRoot) emitError(ERR_MEM, NULL, "Failed to create AST node for directive.");

	
	linedata_ctx linedata = {
		.linenum = directiveToken->linenum,
		.source = ssGetString(directiveToken->sstring)
	};
	// The actions depend on the specific directive

	switch (indexOf(DIRECTIVES, sizeof(DIRECTIVES)/sizeof(DIRECTIVES[0]), directiveToken->lexeme)) {
		case DATA:
		case CONST:
		case BSS:
		case TEXT:
		case EVT:
		case IVT:
		case SET:
		case GLOB:
		case END:
		case STRING:
		case BYTE:
		case HWORD:
		case WORD:
		case FLOAT:
		case ZERO:
		case FILL:
		case ALIGN:
		case SIZE:
		case EXTERN:
		case TYPE:
		case SIZEOF:
		case DEF:
		case INCLUDE:
		case TYPEINFO:
			emitWarning(WARN_UNIMPLEMENTED, &linedata, "Directive `%s` not yet implemented!", directiveToken->lexeme);
			break;
		default:
			emitError(ERR_INVALID_DIRECTIVE, &linedata, "Unknown directive: `%s`", directiveToken->lexeme);
			break;
	}



	addAst(parser, directiveRoot);
}


void parse(Parser* parser) {
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
				parseDirective(parser);
				break;
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
			case TK_MACRO:
			case TK_OUT:
			case TK_IF:
			case TK_MAIN_TYPE:
			case TK_SUB_TYPE:
			default: emitError(ERR_INTERNAL, NULL, "Parser encountered unhandled token type: %d", token->type);
		}

		currentTokenIndex++;
	}

}