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
	TK_D_DATA,
	TK_D_CONST,
	TK_D_BSS,
	TK_D_TEXT,
	TK_D_EVT,
	TK_D_IVT,
	TK_D_SET,
	TK_D_GLOB,
	TK_D_END,
	TK_D_STRING,
	TK_D_BYTE,
	TK_D_HWORD,
	TK_D_WORD,
	TK_D_FLOAT,
	TK_D_ZERO,
	TK_D_FILL,
	TK_D_ALIGN,
	TK_D_SIZE,
	TK_D_EXTERN,
	TK_D_TYPE,
	TK_D_SIZEOF,
	TK_D_DEF,
	TK_D_INCLUDE,
	TK_D_TYPEINFO,
	TK_D_OFFSET,

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

static inline void deleteToken(Token* token);
static inline void deleteToken(Token* token) {
	if (token->lexeme) sdsfree(token->lexeme);
	if (token->sstring) ssDestroySecuredString(token->sstring);
	free(token);
}

#endif