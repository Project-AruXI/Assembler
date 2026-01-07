#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#include "lexer.h"
#include "diagnostics.h"
#include "sds.h"
#include "libsecuredstring.h"


Lexer* initLexer() {
	Lexer* lexer = (Lexer*) malloc(sizeof(Lexer));
	if (!lexer) emitError(ERR_MEM, NULL, "Failed to allocate memory for lexer.");

	lexer->linenum = 0;
	lexer->currentPos = 0;
	lexer->currentChar = '\0';
	lexer->peekedChar = '\0';
	lexer->prevToken = NULL;
	lexer->inScope = false;

	Token** tokens = (Token**) malloc(sizeof(Token*) * 64);
	if (!tokens) emitError(ERR_MEM, NULL, "Failed to allocate memory for token array.");

	lexer->tokens = tokens;
	lexer->tokenCap = 64;
	lexer->tokenCount = 0;

	return lexer;
}

void deinitLexer(Lexer* lexer) {
	for (int i = 0; i < lexer->tokenCount; i++) {
		// log("Freeing token %d: %s (%p)", i, lexer->tokens[i]->lexeme, lexer->tokens[i]->lexeme);
		sdsfree(lexer->tokens[i]->lexeme);
		free(lexer->tokens[i]);
	}
	free(lexer->tokens);
	free(lexer);
}

static void addToken(Lexer* lexer, Token* token) {
	if (lexer->tokenCount == lexer->tokenCap) {
		lexer->tokenCap *= 2;
		Token** newTokens = (Token**) realloc(lexer->tokens, sizeof(Token*) * lexer->tokenCap);
		if (!newTokens) emitError(ERR_MEM, NULL, "Failed to reallocate memory for token array.");
		lexer->tokens = newTokens;
	}
	lexer->tokens[lexer->tokenCount++] = token;

	token->linenum = lexer->linenum;
	token->sstring = ssCreateSecuredString(lexer->line);
}

static void advance(Lexer* lexer) {
	lexer->currentPos++;
	lexer->currentChar = lexer->line[lexer->currentPos];

	// Peek ahead
	lexer->peekedChar = lexer->line[lexer->currentPos + 1];
}

void lexLine(Lexer* lexer, const char* line) {
	initScope("lexLine");

	// The lexer needs the newline at the end to properly insert the NEWLINE token
	// However, each token uses a copy of the lexer's line as the source, which includes the newline
	// To keep the newline but also ignore it, temporaries will be used
	// It also helps making a new string for the source line as `line` is managed by `getline`
	
	const char* originalLine = line; // The line that has the newline
	char* sourceLine = sdstrim(sdsnew(line), "\n \t\r"); // The line to save as the source line

	// log("Lexing line %d: `%s`", lexer->linenum, sourceLine);

	// Since the lexer first uses the line to get the tokens, it needs the original for the newline
	lexer->line = (sds) originalLine;
	lexer->linenum++;
	lexer->currentPos = -1;
	lexer->prevToken = NULL;
	advance(lexer);

	Token* tok = getNextToken(lexer);
	while (tok && tok->type != TK_NEWLINE && tok->type != TK_EOF) {
		if (tok->type == TK_UNKNOWN) {
			// This really should not happen????
			// So it is an internal error????
			linedata_ctx linedata = {
				.linenum = lexer->linenum,
				.source = sourceLine
			};
			emitError(ERR_INTERNAL, &linedata, "Unknown token: %s", tok->lexeme);
		}

		if (tok->type == TK_COMMENT) {
			// Convert comment to newline instead of deleting
			// This is due to the fact that comments can appear after actual text
			// But the newline (important for parser) would be omitted
			// As a tradeoff, this would add unnecessary tokens when the line is just a comment line
			// But the parser can handle consecutive newlines anyway
			// HOWEVER, in the event that closing comments are added, like /* */
			// Then it is imperative that the lexer would have to iterate through the comment until the end
			tok->type = TK_NEWLINE;
			// Even though this token will be marked as a newline, its lexeme remains '%'
			// This will be kept just to see
			break;
		}

		// addToken now needs the source line without the newline
		lexer->line = sourceLine;
		addToken(lexer, tok);
		// printToken(tok);

		lexer->prevToken = tok;
		// Lexer needs the original
		lexer->line = (sds) originalLine;
		tok = getNextToken(lexer);
	}

	// Add the newline token at the end of the line
	// Treat EOF as a newline so there's no `if type == TK_NEWLINE && TK_EOF` many times later
	if (tok && (tok->type == TK_NEWLINE || tok->type == TK_EOF)) {
		if (tok->type == TK_EOF) tok->type = TK_NEWLINE; // No need to change everything else since stuff uses ->type
		lexer->line = sourceLine;
		addToken(lexer, tok);
		// printToken(tok);
	} else if (tok) deleteToken(tok);

	// The line is ephemeral, `line` is managed by getline, and `sourceLine` is managed by the tokens and
	//   the lexer loses ownership of it at the end of the line
	lexer->line = NULL;
}

static void getString(Lexer* lexer, Token* token, linedata_ctx* linedata) {
	int startPos = lexer->currentPos;
	advance(lexer); // consume opening quote

	while (lexer->currentChar != '"' && lexer->currentChar != '\0' && lexer->currentChar != '\n') {
		// Handle escape sequences
		if (lexer->currentChar == '\\' && (lexer->peekedChar == '"' || lexer->peekedChar == '\\')) {
			advance(lexer); // consume '\'
		}
		advance(lexer);
	}

	if (lexer->currentChar != '"') {
		linedata_ctx linedata = {
			.linenum = lexer->linenum,
			.source = lexer->line
		};
		emitError(ERR_INVALID_SYNTAX, &linedata, "Unterminated string literal.");
	}

	int len = lexer->currentPos - startPos + 1; // +1 to include closing quote
	sds lexeme = sdsnewlen(&lexer->line[startPos], len);
	if (!lexeme) emitError(ERR_MEM, NULL, "Failed to allocate memory for token lexeme.");

	token->type = TK_STRING;
	token->lexeme = lexeme;

	advance(lexer); // consume closing quote
}

void getMacroIfOut(Lexer* lexer, Token* token, linedata_ctx* linedata) {
	int startPos = lexer->currentPos;
	advance(lexer); // consume '!'

	while (isalpha(lexer->currentChar)) {
		advance(lexer);
	}

	int len = lexer->currentPos - startPos;
	sds lexeme = sdsnewlen(&lexer->line[startPos], len);
	if (!lexeme) emitError(ERR_MEM, NULL, "Failed to allocate memory for token lexeme.");

	if (strcmp(lexeme, "!macro") == 0) token->type = TK_MACRO;
	else if (strcmp(lexeme, "!out") == 0) token->type = TK_OUT;
	else if (strcmp(lexeme, "!if") == 0) token->type = TK_IF;
	else token->type = TK_UNKNOWN;

	token->lexeme = lexeme;
}

bool isRegister(sds lexeme) {
	// All possible registers are: x0 - x30, sp, ir, lr, xb, v0 - v5, f0-f15, xz,	xr, a0-a9, c0-c4, s1-s10

	if (strcasecmp(lexeme, "sp") == 0 || strcasecmp(lexeme, "ir") == 0 || strcasecmp(lexeme, "lr") == 0 ||
		strcasecmp(lexeme, "xb") == 0 || strcasecmp(lexeme, "xz") == 0 || strcasecmp(lexeme, "xr") == 0) return true;

	if (!isdigit(lexeme[1])) return false;

	if (lexeme[0] == 'x' || lexeme[0] == 'X') {
		// x0 - x30
		int regNum = atoi(&lexeme[1]);
		if (regNum >= 0 && regNum <= 30 && (regNum == 0 || lexeme[2] == '\0' || (regNum >= 10 && lexeme[3] == '\0'))) {
			return true;
		}
	} else if (lexeme[0] == 'v' || lexeme[0] == 'V') {
		// v0 - v5
		int regNum = atoi(&lexeme[1]);
		if (regNum >= 0 && regNum <= 5 && (regNum == 0 || lexeme[2] == '\0')) {
			return true;
		}
	} else if (lexeme[0] == 'f' || lexeme[0] == 'F') {
		// f0 - f15
		int regNum = atoi(&lexeme[1]);
		if (regNum >= 0 && regNum <= 15 && (regNum == 0 || lexeme[2] == '\0' || (regNum >= 10 && lexeme[3] == '\0'))) {
			return true;
		}
	} else if (lexeme[0] == 'a' || lexeme[0] == 'A') {
		// a0 - a9
		int regNum = atoi(&lexeme[1]);
		if (regNum >= 0 && regNum <= 9 && (regNum == 0 || lexeme[2] == '\0')) {
			return true;
		}
	} else if (lexeme[0] == 'c' || lexeme[0] == 'C') {
		// c0 - c4
		int regNum = atoi(&lexeme[1]);
		if (regNum >= 0 && regNum <= 4 && (regNum == 0 || lexeme[2] == '\0')) {
			return true;
		}
	} else if (lexeme[0] == 's' || lexeme[0] == 'S') {
		// s1 - s10
		int regNum = atoi(&lexeme[1]);
		if (regNum >= 1 && regNum <= 10 && (regNum <= 9 || lexeme[3] == '\0')) {
			return true;
		}
	}

	return false;
}

Token* getNextToken(Lexer* lexer) {
	linedata_ctx linedata = {
		.linenum = lexer->linenum,
		.source = lexer->line
	};

	Token* token = (Token*) malloc(sizeof(Token));
	if (!token) emitError(ERR_MEM, NULL, "Failed to allocate memory for token.");
	token->lexeme = NULL;
	token->type = TK_UNKNOWN;
	token->sstring = NULL;
	token->linenum = -1;

	while (isblank(lexer->currentChar)) {
		advance(lexer);
	}

	// debug(DEBUG_TRACE, "Current char: '%c' (0x%x) at pos %d", lexer->currentChar, lexer->currentChar, lexer->currentPos);

	switch (lexer->currentChar) {
		case '%':
			token->type = TK_COMMENT;
			token->lexeme = sdsnew("%");
			return token;
		case '\n':
			token->type = TK_NEWLINE;
			token->lexeme = sdsnew("NEWLINE");
			return token;
		case '\0':
			token->type = TK_EOF;
			token->lexeme = sdsnew("EOF");
			return token;
		case '.':
			// Reading a dot has many interpretations
			// - A directive (.data, .text, etc)
			// - A struct/union member access (e.g. myStruct.member)
			// - A floating point number starting with . (.5, .25, etc)
			// - The end of .def statement (data:8.)
			// All other instances except for floating point and directive are kept as single
			if (isalpha(lexer->peekedChar)) {
				char prevChar = lexer->currentPos > 0 ? lexer->line[lexer->currentPos - 1] : '\0';
				// It can a member access, use the previous token or character for context
				if (lexer->prevToken && (lexer->prevToken->type == TK_MAIN_TYPE || !isspace(prevChar))) {
					// Member access
					token->type = TK_DOT;
					token->lexeme = sdsnew(".");
					// log("%p; %s", token->lexeme, token->lexeme);
					advance(lexer);
					return token;
				} else if (lexer->inScope) { // The end of a .def statement when consecutive
					token->type = TK_DOT;
					token->lexeme = sdsnew(".");
					advance(lexer);
					return token;
				}

				// Otherwise, directive
				int startPos = lexer->currentPos;
				advance(lexer); // consume '.'

				while (isalpha(lexer->currentChar)) {
					advance(lexer);
				}

				int len = lexer->currentPos - startPos;
				sds lexeme = sdsnewlen(&lexer->line[startPos], len);
				if (!lexeme) emitError(ERR_MEM, NULL, "Failed to allocate memory for token lexeme.");

				token->type = TK_DIRECTIVE;
				token->lexeme = lexeme;
				return token;
			} else if (isdigit(lexer->peekedChar)) {
				// FP number starting with .
				int startPos = lexer->currentPos;
				advance(lexer); // consume '.'

				while (isdigit(lexer->currentChar)) {
					advance(lexer);
				}

				int len = lexer->currentPos - startPos;
				sds lexeme = sdsnewlen(&lexer->line[startPos], len);
				if (!lexeme) emitError(ERR_MEM, NULL, "Failed to allocate memory for token lexeme.");

				token->type = TK_FLOAT;
				token->lexeme = lexeme;
				return token;
			}
			// Single dot
			token->type = TK_DOT;
			token->lexeme = sdsnew(".");
			// log("%p; %s", token->lexeme, token->lexeme);
			advance(lexer);
			return token;
		case ',':
			token->type = TK_COMMA;
			token->lexeme = sdsnew(",");
			// log("%p; %s", token->lexeme, token->lexeme);
			// sds_free(token->lexeme);
			advance(lexer);
			return token;
		case '"':
			getString(lexer, token, &linedata);
			return token;
		case '\'':
			// Character literal
			advance(lexer); // consume opening quote
			// char ch = lexer->currentChar;
			advance(lexer); // consume character
			if (lexer->currentChar != '\'') {
				emitError(ERR_INVALID_SYNTAX, &linedata, "Unterminated character literal.");
			}
			advance(lexer); // consume closing quote

			token->type = TK_CHAR;
			sds lexeme = sdsnewlen(&lexer->line[lexer->currentPos-2], 1);
			if (!lexeme) emitError(ERR_MEM, NULL, "Failed to allocate memory for token lexeme.");
			token->lexeme = lexeme;
			return token;
		case '+':
			token->type = TK_PLUS;
			token->lexeme = sdsnew("+");
			advance(lexer);
			return token;
		case '-':
			token->type = TK_MINUS;
			token->lexeme = sdsnew("-");
			advance(lexer);
			return token;
		case '*':
			token->type = TK_ASTERISK;
			token->lexeme = sdsnew("*");
			advance(lexer);
			return token;
		case '/':
			token->type = TK_DIVIDE;
			token->lexeme = sdsnew("/");
			advance(lexer);
			return token;
		case '(':
			token->type = TK_LPAREN;
			token->lexeme = sdsnew("(");
			advance(lexer);
			return token;
		case ')':
			token->type = TK_RPAREN;
			token->lexeme = sdsnew(")");
			advance(lexer);
			return token;
		case '[':
			token->type = TK_LSQBRACKET;
			token->lexeme = sdsnew("[");
			advance(lexer);
			return token;
		case ']':
			token->type = TK_RSQBRACKET;
			token->lexeme = sdsnew("]");
			advance(lexer);
			return token;
		case '{':
			token->type = TK_LBRACKET;
			token->lexeme = sdsnew("{");
			advance(lexer);
			lexer->inScope = true;
			return token;
		case '}':
			token->type = TK_RBRACKET;
			token->lexeme = sdsnew("}");
			advance(lexer);
			lexer->inScope = false;
			return token;
		case '#':
			// '#' indicates an immediate value, needs to be followed by an integer
			// otherwise, it is invalid
			if (isalnum(lexer->peekedChar) || lexer->peekedChar == '-' || lexer->peekedChar == '+') {
				advance(lexer); // consume '#'
				int startPos = lexer->currentPos;

				while (isalnum(lexer->currentChar)) {
					advance(lexer);
				}

				int len = lexer->currentPos - startPos;
				sds lexeme = sdsnewlen(&lexer->line[startPos-1], len+1); // +1 to include '#'
				if (!lexeme) emitError(ERR_MEM, NULL, "Failed to allocate memory for token lexeme.");

				token->type = TK_IMM;
				token->lexeme = lexeme;
				return token;
			}

			emitError(ERR_INVALID_SYNTAX, &linedata, "Unexpected character after '#': '%c'", lexer->peekedChar);
		case ':':
			if (lexer->peekedChar == ':') {
				token->type = TK_COLON_COLON;
				token->lexeme = sdsnew("::");
				advance(lexer);
				advance(lexer);
			} else {
				token->type = TK_COLON;
				token->lexeme = sdsnew(":");
				advance(lexer);
			}
			return token;
		case '=':
			if (ispunct(lexer->peekedChar) && lexer->peekedChar != '_') {
				// Anything other than letters, numbers, and `_` is invalid
				emitError(ERR_INVALID_SYNTAX, &linedata, "Unexpected character: '%c'", lexer->peekedChar);
			}

			token->type = TK_LITERAL;
			token->lexeme = sdsnew("=");
			advance(lexer);
			return token;
		case '!':
			if (isalpha(lexer->peekedChar)) {
				getMacroIfOut(lexer, token, &linedata);
				return token;
			} else {
				// Single '!' is not valid in this assembly language
				emitError(ERR_INVALID_SYNTAX, &linedata, "Unexpected character: '%c'", lexer->currentChar);
			}
			return token;
		case '&':
			token->type = TK_BITWISE_AND;
			token->lexeme = sdsnew("&");
			advance(lexer);
			return token;
		case '|':
			token->type = TK_BITWISE_OR;
			token->lexeme = sdsnew("|");
			advance(lexer);
			return token;
		case '^':
			token->type = TK_BITWISE_XOR;
			token->lexeme = sdsnew("^");
			advance(lexer);
			return token;
		case '~':
			token->type = TK_BITWISE_NOT;
			token->lexeme = sdsnew("~");
			advance(lexer);
			return token;
		case '<':
			if (lexer->peekedChar == '<') {
				token->type = TK_BITWISE_SL;
				token->lexeme = sdsnew("<<");
				advance(lexer);
				advance(lexer);
				return token;
			} else {
				emitError(ERR_INVALID_SYNTAX, &linedata, "Unexpected character: '%c'. Did you mean '<<'?", lexer->currentChar);
			}
			return token;
		case '>':
			if (lexer->peekedChar == '>') {
				token->type = TK_BITWISE_SR;
				token->lexeme = sdsnew(">>");
				advance(lexer);
				advance(lexer);
				return token;
			} else {
				emitError(ERR_INVALID_SYNTAX, &linedata, "Unexpected character: '%c'. Did you mean '>>'?", lexer->currentChar);
			}
			return token;
		case '@':
			advance(lexer);

			if (isalpha(lexer->currentChar) || lexer->currentChar == '_') {
				// Macro arg/param (ie @reg)
				int startPos = lexer->currentPos;
				while (isalnum(lexer->currentChar) || lexer->currentChar == '_') {
					advance(lexer);
				}
				int len = lexer->currentPos - startPos;
				sds lexeme = sdsnewlen(&lexer->line[startPos-1], len+1); // +1 to include '@'
				if (!lexeme) emitError(ERR_MEM, NULL, "Failed to allocate memory for token lexeme.");
				token->type = TK_MACRO_ARG;
				token->lexeme = lexeme;
				return token;
			} else if (lexer->currentChar == '-' || lexer->currentChar == '+' || isspace(lexer->currentChar)) {
				// Behaves as the LP (ie @-LABEL)
				token->type = TK_LP;
				token->lexeme = sdsnew("@");
				return token;
			} else {
				emitError(ERR_INVALID_SYNTAX, &linedata, "Unexpected character after '@': '%c'", lexer->currentChar);
			}
		case '$':
			if (isalpha(lexer->peekedChar)) {
				int startPos = lexer->currentPos;
				advance(lexer); // consume '$'

				while (isalnum(lexer->currentChar)) {
					advance(lexer);
				}

				int len = lexer->currentPos - startPos;
				sds lexeme = sdsnewlen(&lexer->line[startPos], len);
				if (!lexeme) emitError(ERR_MEM, NULL, "Failed to allocate memory for token lexeme.");

				token->type = TK_MAIN_TYPE;
				token->lexeme = lexeme;
				return token;
			}

			// Single '$' is not valid
			emitError(ERR_INVALID_SYNTAX, &linedata, "Unexpected character: '%c'", lexer->currentChar);
		default:
			if (isalpha(lexer->currentChar) || lexer->currentChar == '_') {
				// Identifier, label, or register
				int startPos = lexer->currentPos;
				while (isalnum(lexer->currentChar) || lexer->currentChar == '_') {
					advance(lexer);
				}
				int len = lexer->currentPos - startPos;
				sds lexeme = sdsnewlen(&lexer->line[startPos], len);
				if (!lexeme) emitError(ERR_MEM, NULL, "Failed to allocate memory for token lexeme.");

				token->lexeme = lexeme;
				if (lexer->currentChar == ':') {
					// What if it is `[identifier] :(:)`, cannot use currentChar to determine
					if (lexer->inScope) {
						// Treat [...]:(:) as [identifier]:(:) instead of [label]:
						token->type = TK_IDENTIFIER;
						return token;
					}
					// Label
					token->type = TK_LABEL;
					advance(lexer); // consume ':'
				} else if (isRegister(lexeme)) {
					// Register
					token->type = TK_REGISTER;
				} else {
					// For now, all others are identifiers
					token->type = TK_IDENTIFIER;
				}
				return token;
			} else if (isdigit(lexer->currentChar)) {
				// Number (integer or float)
				int startPos = lexer->currentPos;

				// Check for hexadecimal
				if (lexer->currentChar == '0' && (lexer->peekedChar == 'x' || lexer->peekedChar == 'X')) {
					advance(lexer); // consume '0'
					advance(lexer); // consume 'x'|'X'

					int hexStart = lexer->currentPos;
					while (isxdigit(lexer->currentChar)) {
						advance(lexer);
					}

					// It can be the case that there is a non-hex character (outside of a-f and 0-9)
					//   .ie 0xagg, it needs to be checked
					if (isalpha(lexer->currentChar) || lexer->currentChar == '_') {
						emitError(ERR_INVALID_SYNTAX, &linedata, "Invalid hexadecimal literal: unexpected character '%c' after hex digits.", lexer->currentChar);
					}

					int hexLen = lexer->currentPos - hexStart;
					if (hexLen == 0) emitError(ERR_INVALID_SYNTAX, &linedata, "Invalid hexadecimal literal: missing digits after '0x'.");	

					int len = lexer->currentPos - startPos;
					sds lexeme = sdsnewlen(&lexer->line[startPos], len);
					if (!lexeme) emitError(ERR_MEM, NULL, "Failed to allocate memory for token lexeme.");

					token->type = TK_INTEGER;
					token->lexeme = lexeme;

					return token;
				} else if (lexer->currentChar == '0' && (lexer->peekedChar == 'b' || lexer->peekedChar == 'B')) {
					advance(lexer); // consume '0'
					advance(lexer); // consume 'b'|'B'

					int binStart = lexer->currentPos;
					while (lexer->currentChar == '0' || lexer->currentChar == '1') {
						advance(lexer);
					}

					// It can be the case that there is a non-binary character (outside of 0 and 1)
					//   .ie 0b102, it needs to be checked
					if (isalpha(lexer->currentChar) || isdigit(lexer->currentChar) || lexer->currentChar == '_') {
						emitError(ERR_INVALID_SYNTAX, &linedata, "Invalid binary literal: unexpected character '%c' after binary digits.", lexer->currentChar);
					}

					int binLen = lexer->currentPos - binStart;
					if (binLen == 0) emitError(ERR_INVALID_SYNTAX, &linedata, "Invalid binary literal: missing digits after '0b'.");	

					int len = lexer->currentPos - startPos;
					sds lexeme = sdsnewlen(&lexer->line[startPos], len);
					if (!lexeme) emitError(ERR_MEM, NULL, "Failed to allocate memory for token lexeme.");

					token->type = TK_INTEGER;
					token->lexeme = lexeme;

					return token;
				} else {
					while (isdigit(lexer->currentChar)) {
						advance(lexer);
					}
				}
				if (lexer->currentChar == '.' && (lexer->prevToken && lexer->prevToken->type != TK_COLON)) {
					// Floating point number
					advance(lexer); // consume '.'
					while (isdigit(lexer->currentChar)) {
						advance(lexer);
					}
					int len = lexer->currentPos - startPos;
					sds lexeme = sdsnewlen(&lexer->line[startPos], len);
					if (!lexeme) emitError(ERR_MEM, NULL, "Failed to allocate memory for token lexeme.");
					
					token->type = TK_FLOAT;
					token->lexeme = lexeme;
					return token;
				} else if (lexer->currentChar == '.' && (lexer->prevToken && lexer->prevToken->type == TK_COLON)) {
					// End of a .def statement (data:8.)
					int len = lexer->currentPos - startPos;
					sds lexeme = sdsnewlen(&lexer->line[startPos], len);
					if (!lexeme) emitError(ERR_MEM, NULL, "Failed to allocate memory for token lexeme.");
					
					token->type = TK_INTEGER;
					token->lexeme = lexeme;
					return token;

					// Next token should be dot
				} else {
					// Integer
					int len = lexer->currentPos - startPos;
					sds lexeme = sdsnewlen(&lexer->line[startPos], len);
					if (!lexeme) emitError(ERR_MEM, NULL, "Failed to allocate memory for token lexeme.");
					
					token->type = TK_INTEGER;
					token->lexeme = lexeme;
					return token;
				}
			} else {
				// Unknown character
				emitError(ERR_INVALID_SYNTAX, &linedata, "Unexpected character: '%c'", lexer->currentChar);
			}
			break;
	}

	return NULL;
}

/**
 * The following functions primarily serve as helpers to expose certain information, such as printing tokens,
 * retrieving tokens from the lexer, or providing debugging output. They do not participate in the main
 * tokenization process but are useful for interacting with or inspecting the lexer's state and output.
 */

void printToken(Token* token) {
	if (!token) {
		rlog("NULL token\n");
		return;
	}

	const char* typeStr;
	switch (token->type) {
		case TK_EOF: typeStr = "TK_EOF"; break;
		case TK_NEWLINE: typeStr = "TK_NEWLINE"; break;
		case TK_LABEL: typeStr = "TK_LABEL"; break;
		case TK_IDENTIFIER: typeStr = "TK_IDENTIFIER"; break;
		case TK_DIRECTIVE: typeStr = "TK_DIRECTIVE"; break;
		case TK_INSTRUCTION: typeStr = "TK_INSTRUCTION"; break;
		case TK_REGISTER: typeStr = "TK_REGISTER"; break;
		case TK_IMM: typeStr = "TK_IMM"; break;
		case TK_COMMA: typeStr = "TK_COMMA"; break;
		case TK_LPAREN: typeStr = "TK_LPAREN"; break;
		case TK_RPAREN: typeStr = "TK_RPAREN"; break;
		case TK_LBRACKET: typeStr = "TK_LBRACKET"; break;
		case TK_RBRACKET: typeStr = "TK_RBRACKET"; break;
		case TK_LSQBRACKET: typeStr = "TK_LSQBRACKET"; break;
		case TK_RSQBRACKET: typeStr = "TK_RSQBRACKET"; break;
		case TK_COLON: typeStr = "TK_COLON"; break;
		case TK_COLON_COLON: typeStr = "TK_COLON_COLON"; break;
		case TK_STRING: typeStr = "TK_STRING"; break;
		case TK_DOT: typeStr = "TK_DOT"; break;
		case TK_PLUS: typeStr = "TK_PLUS"; break;
		case TK_MINUS: typeStr = "TK_MINUS"; break;
		case TK_ASTERISK: typeStr = "TK_ASTERISK"; break;
		case TK_DIVIDE: typeStr = "TK_DIVIDE"; break;
		case TK_COMMENT: typeStr = "TK_COMMENT"; break;
		case TK_LITERAL: typeStr = "TK_LITERAL"; break;
		case TK_BITWISE_AND: typeStr = "TK_BITWISE_AND"; break;
		case TK_BITWISE_OR: typeStr = "TK_BITWISE_OR"; break;
		case TK_BITWISE_XOR: typeStr = "TK_BITWISE_XOR"; break;
		case TK_BITWISE_NOT: typeStr = "TK_BITWISE_NOT"; break;
		case TK_BITWISE_SL: typeStr = "TK_BITWISE_SL"; break;
		case TK_BITWISE_SR: typeStr = "TK_BITWISE_SR"; break;
		case TK_LP: typeStr = "TK_LP"; break;
		case TK_MACRO_ARG: typeStr = "TK_MACRO_ARG"; break;
		case TK_INTEGER: typeStr = "TK_INTEGER"; break;
		case TK_FLOAT: typeStr = "TK_FLOAT"; break;
		case TK_CHAR: typeStr = "TK_CHAR"; break;
		case TK_MACRO: typeStr = "TK_MACRO"; break;
		case TK_OUT: typeStr = "TK_OUT"; break;
		case TK_IF: typeStr = "TK_IF"; break;
		case TK_MAIN_TYPE: typeStr = "TK_MAIN_TYPE"; break;
		case TK_SUB_TYPE: typeStr = "TK_SUB_TYPE"; break;
		case TK_UNKNOWN: typeStr = "TK_UNKNOWN"; break;
		default: typeStr = "UNKNOWN_TYPE"; break;
	}

	rlog("Token(type=%s, lexeme=`%s`, line=%d)", typeStr, token->lexeme, token->linenum);
}

Token* getToken(Lexer* lexer, int index) {
	if (index < 0 || index >= lexer->tokenCount) return NULL;
	return lexer->tokens[index];
}

void resetLexer(Lexer* lexer) {
	for (int i = 0; i < lexer->tokenCount; i++) {
		sdsfree(lexer->tokens[i]->lexeme);
		free(lexer->tokens[i]);
	}
	// Even though tokens were freed, capacity is to remain

	lexer->tokenCount = 0;
	lexer->currentChar = '\0';
	lexer->peekedChar = '\0';
	lexer->inScope = false;
	lexer->linenum = 0;
	lexer->line = NULL;

	// prevToken and currentPos are already reset in lexLine
}