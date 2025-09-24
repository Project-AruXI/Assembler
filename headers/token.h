#ifndef _TOKEN_H_
#define _TOKEN_H_

#include "../common/libsecuredstring.h"
#include "../common/sds.h"

typedef enum {
	TK_EOF,
	TK_NEWLINE,
	TK_LABEL, // [label]:
	TK_IDENTIFIER, // [identifier] (used for .type names, ie `.type Node, $obj` or .def, ie `next::Node.)
	TK_DIRECTIVE, // .[directive]
	TK_INSTRUCTION, // [instruction]
	TK_REGISTER, // [register]
	TK_IMM, // #[immediate]
	TK_COMMA, // ,
	TK_LPAREN, // (
	TK_RPAREN, // )
	TK_LSQBRACKET, // [
	TK_RSQBRACKET, // ]
	TK_LBRACKET, // {
	TK_RBRACKET, // }
	TK_COLON, // :
	TK_COLON_COLON, // ::
	TK_STRING, // "[string]"
	TK_DOT, // .
	TK_PLUS, // +
	TK_MINUS, // -
	TK_ASTERISK, // *
	TK_DIVIDE, // /
	TK_COMMENT, // %
	TK_LD_IMM, // =
	TK_BITWISE_AND, // &
	TK_BITWISE_OR, // |
	TK_BITWISE_XOR, // ^
	TK_BITWISE_NOT, // ~
	TK_LP, // @
	TK_MACRO_ARG, // @[macro_arg]
	TK_INTEGER, // [hex|bin|dec]
	TK_FLOAT, // [float]
	TK_CHAR, // '[char]'
	TK_MACRO, // !macro
	TK_OUT, // !out
	TK_IF, // !if
	TK_MAIN_TYPE, // $[type]
	TK_SUB_TYPE, // [subtype]
	TK_UNKNOWN
} tokenType;



typedef struct Token {
	sds lexeme;
	tokenType type;
	int linenum;
	SString* sstring;
} Token;

void deleteToken(Token* token);
void deleteToken(Token* token) {
	if (token->lexeme) sdsfree(token->lexeme);
	if (token->sstring) ssDestroySecuredString(token->sstring);
	free(token);
}

#endif