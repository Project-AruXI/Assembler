#ifndef _LEXER_H_
#define _LEXER_H_

#include "token.h"

typedef struct Lexer {
	const char* line;
	int linenum;
	Token** tokens;
	int tokenCount;
	int tokenCap;

	char currentChar;
	int currentPos;
} Lexer;

Lexer* initLexer();
void deinitLexer(Lexer* lexer);

void lexLine(Lexer* lexer, const char* line);

Token* getNextToken(Lexer* lexer);

#endif