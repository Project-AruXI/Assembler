#ifndef _LEXER_H_
#define _LEXER_H_

#include <stdbool.h>

#include "token.h"


typedef struct Lexer {
	const char* line;
	int linenum;

	Token** tokens;
	int tokenCount;
	int tokenCap;

	char currentChar;
	int currentPos;
	char peekedChar;

	Token* prevToken;

	bool inScope;
} Lexer;


/**
 * Initializes the lexer.
 * @return The lexer
 */
Lexer* initLexer();
/**
 * Frees the lexer.
 * @param lexer The lexer to deinitialize
 */
void deinitLexer(Lexer* lexer);

/**
 * Lexes a line of code, producing tokens, and adding them to the lexer's token list.
 * @param lexer The lexer
 * @param line The line to lex
 */
void lexLine(Lexer* lexer, const char* line);

/**
 * Retrieves the next token from the source line. The state of the lexer is updated via
 * the list of tokens stored and the position of the current character.
 * @param lexer The lexer
 * @return The next token
 */
Token* getNextToken(Lexer* lexer);

/* The following functions are utility functions that are not essential to the core logic or functioning of the lexer. */

/**
 * Prints the given token.
 * @param token The token to print
 */
void printToken(Token* token);

/**
 * Gets a token from the token list by index.
 * @param lexer The lexer
 * @param index The index of the token to get
 * @return The token at the given index
 */
Token* getToken(Lexer* lexer, int index);

/**
 * Resets the lexer state, clearing the token list and resetting position.
 * This is only useful for testing purposes. Do not use in actual assembler run.
 * @param lexer The lexer to reset
 */
void resetLexer(Lexer* lexer);

#endif