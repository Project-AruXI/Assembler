#include <stdlib.h>
#include <ctype.h>

#include "lexer.h"
#include "diagnostics.h"

Lexer* initLexer() {
	Lexer* lexer = (Lexer*) malloc(sizeof(Lexer));
	if (!lexer) emitError(ERR_MEM, NULL, "Failed to allocate memory for lexer.");

	lexer->linenum = 0;
	lexer->currentPos = 0;
	lexer->currentChar = '\0';

	Token** tokens = (Token**) malloc(sizeof(Token*) * 64);
	if (!tokens) emitError(ERR_MEM, NULL, "Failed to allocate memory for token array.");

	lexer->tokens = tokens;
	lexer->tokenCap = 64;
	lexer->tokenCount = 0;

	return lexer;
}

void deinitLexer(Lexer* lexer) {
	for (int i = 0; i < lexer->tokenCount; i++) {
		free(lexer->tokens[i]->lexeme);
		free(lexer->tokens[i]);
	}
	free(lexer->tokens);
	free(lexer);
}

static void addToken(Lexer* lexer, Token* token) {

}

static void advance(Lexer* lexer) {
	lexer->currentPos++;
	lexer->currentChar = lexer->line[lexer->currentPos];
}

void lexLine(Lexer* lexer, const char* line) {
	initScope("lexLine");

	log("Lexing line %d: %s", lexer->linenum, line);

	lexer->line = line;
	lexer->linenum++;
	lexer->currentPos = -1;
	advance(lexer);


	Token* tok = getNextToken(lexer);
	while (tok && tok->type != TK_NEWLINE) {
		if (tok->type == TK_COMMENT) {
			log("Comment found, ignoring rest of line.");
			free(tok);
			return;
		}

		tok = getNextToken(lexer);
	}
}

Token* getNextToken(Lexer* lexer) {
	Token* token = (Token*) malloc(sizeof(Token));

	while (isblank(lexer->currentChar)) {
		advance(lexer);
	}

	switch (lexer->currentChar) {
		case '%':
			token->type = TK_COMMENT;
			token->lexeme = "%";
			return token;
		case '\n':
			token->type = TK_NEWLINE;
			token->lexeme = "\n";
			return token;
	}

	return token;
}