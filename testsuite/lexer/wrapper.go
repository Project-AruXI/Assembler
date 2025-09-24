package lexerTests

/*
#cgo LDFLAGS: -L../../out -llexer
#include <stdlib.h>
#include "../../headers/lexer.h"
#include "../../headers/diagnostics.h"
#include "../../headers/token.h"
#include "../../common/libsecuredstring.h"
#include "../../common/sds.h"
*/
import "C"
import (
	"fmt"
	"unsafe"
)


func lexerInitLexer() *C.Lexer {
	lexer := C.initLexer()
	return lexer
}

func lexerDeinitLexer(lexer *C.Lexer) {
	C.deinitLexer(lexer)
}

func lexerLexLine(lexer *C.Lexer, line string) {
	cLine := C.CString(line)
	defer C.free(unsafe.Pointer(cLine))
	C.lexLine(lexer, cLine)
}

func lexerGetNextToken(lexer *C.Lexer) *C.Token {
	return C.getNextToken(lexer)
}

func lexerGetToken(lexer *C.Lexer, index int) *C.Token {
	return C.getToken(lexer, C.int(index))
}

func lexerResetLexer(lexer *C.Lexer) {
	C.resetLexer(lexer)
}


func assertToken(t *C.Token, expectedType C.tokenType, expectedLexeme string) bool {
	if t._type != expectedType {
		return false
	}
	lexeme := C.GoString(t.lexeme)
	if lexeme != expectedLexeme {
		return false
	}

	return true
}

func fromCharPtrToGoString(cstr *C.char) string {
	return C.GoString(cstr)
}

// Function to convert token type to string for better logging
func tokenTypeToString(t C.tokenType) string {
	switch t {
		case C.TK_EOF:
			return "TK_EOF"
		case C.TK_NEWLINE:
			return "TK_NEWLINE"
		case C.TK_LABEL:
			return "TK_LABEL"
		case C.TK_IDENTIFIER:
			return "TK_IDENTIFIER"
		case C.TK_DIRECTIVE:
			return "TK_DIRECTIVE"
		case C.TK_INSTRUCTION:
			return "TK_INSTRUCTION"
		case C.TK_REGISTER:
			return "TK_REGISTER"
		case C.TK_IMM:
			return "TK_IMM"
		case C.TK_COMMA:
			return "TK_COMMA"
		case C.TK_LPAREN:
			return "TK_LPAREN"
		case C.TK_RPAREN:
			return "TK_RPAREN"
		case C.TK_LSQBRACKET:
			return "TK_LSQBRACKET"
		case C.TK_RSQBRACKET:
			return "TK_RSQBRACKET"
		case C.TK_LBRACKET:
			return "TK_LBRACKET"
		case C.TK_RBRACKET:
			return "TK_RBRACKET"
		case C.TK_COLON:
			return "TK_COLON"
		case C.TK_COLON_COLON:
			return "TK_COLON_COLON"
		case C.TK_STRING:
			return "TK_STRING"
		case C.TK_DOT:
			return "TK_DOT"
		case C.TK_PLUS:
			return "TK_PLUS"
		case C.TK_MINUS:
			return "TK_MINUS"
		case C.TK_ASTERISK:
			return "TK_ASTERISK"
		case C.TK_DIVIDE:
			return "TK_DIVIDE"
		case C.TK_COMMENT:
			return "TK_COMMENT"
		case C.TK_LD_IMM:
			return "TK_LD_IMM"
		case C.TK_BITWISE_AND:
			return "TK_BITWISE_AND"
		case C.TK_BITWISE_OR:
			return "TK_BITWISE_OR"
		case C.TK_BITWISE_XOR:
			return "TK_BITWISE_XOR"
		case C.TK_BITWISE_NOT:
			return "TK_BITWISE_NOT"
		case C.TK_LP:
			return "TK_LP"
		case C.TK_MACRO_ARG:
			return "TK_MACRO_ARG"
		case C.TK_INTEGER:
			return "TK_INTEGER"
		case C.TK_FLOAT:
			return "TK_FLOAT"
		case C.TK_CHAR:
			return "TK_CHAR"
		case C.TK_MACRO:
			return "TK_MACRO"
		case C.TK_OUT:
			return "TK_OUT"
		case C.TK_IF:
			return "TK_IF"
		case C.TK_MAIN_TYPE:
			return "TK_MAIN_TYPE"
		case C.TK_SUB_TYPE:
			return "TK_SUB_TYPE"
		default:
			return "UNKNOWN_TOKEN"
	}
}


const (
	RESET = "\033[0m"
	RED = "\033[31m"
	GREEN = "\033[32m"
	YELLOW = "\033[33m"
	BLUE = "\033[34m"
	MAGENTA = "\033[35m"
	CYAN = "\033[36m"
	WHITE = "\033[37m"

	BRIGHT_BLACK = "\033[90m"
	BRIGHT_RED = "\033[91m"
	BRIGHT_GREEN = "\033[92m"
	BRIGHT_YELLOW = "\033[93m"
	BRIGHT_BLUE = "\033[94m"
	BRIGHT_MAGENTA = "\033[95m"
	BRIGHT_CYAN = "\033[96m"
	BRIGHT_WHITE = "\033[97m"
)




var (
	TK_EOF           int
	TK_NEWLINE       int
	TK_LABEL         int
	TK_IDENTIFIER    int
	TK_DIRECTIVE     int
	TK_INSTRUCTION   int
	TK_REGISTER      int
	TK_IMM           int
	TK_COMMA         int
	TK_LPAREN        int
	TK_RPAREN        int
	TK_LSQBRACKET    int
	TK_RSQBRACKET    int
	TK_LBRACKET      int
	TK_RBRACKET      int
	TK_COLON         int
	TK_COLON_COLON  int
	TK_STRING        int
	TK_DOT           int
	TK_PLUS          int
	TK_MINUS         int
	TK_ASTERISK      int
	TK_DIVIDE        int
	TK_COMMENT       int
	TK_LD_IMM        int
	TK_BITWISE_AND   int
	TK_BITWISE_OR    int
	TK_BITWISE_XOR   int
	TK_BITWISE_NOT   int
	TK_LP            int
	TK_MACRO_ARG     int
	TK_INTEGER       int
	TK_FLOAT         int
	TK_CHAR          int
	TK_MACRO         int
	TK_OUT           int
	TK_IF            int
	TK_DEF           int
	TK_MAIN_TYPE     int
	TK_SUB_TYPE      int
)

func tokenTypeInit() {
	TK_EOF           = int(C.tokenType(C.TK_EOF))
	TK_NEWLINE       = int(C.tokenType(C.TK_NEWLINE))
	TK_LABEL         = int(C.tokenType(C.TK_LABEL))
	TK_IDENTIFIER    = int(C.tokenType(C.TK_IDENTIFIER))
	TK_DIRECTIVE     = int(C.tokenType(C.TK_DIRECTIVE))
	TK_INSTRUCTION   = int(C.tokenType(C.TK_INSTRUCTION))
	TK_REGISTER      = int(C.tokenType(C.TK_REGISTER))
	TK_IMM           = int(C.tokenType(C.TK_IMM))
	TK_COMMA         = int(C.tokenType(C.TK_COMMA))
	TK_LPAREN        = int(C.tokenType(C.TK_LPAREN))
	TK_RPAREN        = int(C.tokenType(C.TK_RPAREN))
	TK_LSQBRACKET    = int(C.tokenType(C.TK_LSQBRACKET))
	TK_RSQBRACKET    = int(C.tokenType(C.TK_RSQBRACKET))
	TK_LBRACKET      = int(C.tokenType(C.TK_LBRACKET))
	TK_RBRACKET      = int(C.tokenType(C.TK_RBRACKET))
	TK_COLON         = int(C.tokenType(C.TK_COLON))
	TK_COLON_COLON   = int(C.tokenType(C.TK_COLON_COLON))
	TK_STRING        = int(C.tokenType(C.TK_STRING))
	TK_DOT           = int(C.tokenType(C.TK_DOT))
	TK_PLUS          = int(C.tokenType(C.TK_PLUS))
	TK_MINUS         = int(C.tokenType(C.TK_MINUS))
	TK_ASTERISK      = int(C.tokenType(C.TK_ASTERISK))
	TK_DIVIDE        = int(C.tokenType(C.TK_DIVIDE))
	TK_COMMENT       = int(C.tokenType(C.TK_COMMENT))
	TK_LD_IMM        = int(C.tokenType(C.TK_LD_IMM))
	TK_BITWISE_AND   = int(C.tokenType(C.TK_BITWISE_AND))
	TK_BITWISE_OR    = int(C.tokenType(C.TK_BITWISE_OR))
	TK_BITWISE_XOR   = int(C.tokenType(C.TK_BITWISE_XOR))
	TK_BITWISE_NOT   = int(C.tokenType(C.TK_BITWISE_NOT))
	TK_LP            = int(C.tokenType(C.TK_LP))
	TK_MACRO_ARG     = int(C.tokenType(C.TK_MACRO_ARG))
	TK_INTEGER       = int(C.tokenType(C.TK_INTEGER))
	TK_FLOAT         = int(C.tokenType(C.TK_FLOAT))
	TK_CHAR          = int(C.tokenType(C.TK_CHAR))
	TK_MACRO         = int(C.tokenType(C.TK_MACRO))
	TK_OUT           = int(C.tokenType(C.TK_OUT))
	TK_IF            = int(C.tokenType(C.TK_IF))
	TK_MAIN_TYPE     = int(C.tokenType(C.TK_MAIN_TYPE))
	TK_SUB_TYPE      = int(C.tokenType(C.TK_SUB_TYPE))
}

// Function where, given an array of struct {_type int, lexeme string}, it loops through it, printing each
func printTokenArray(tokens []struct{_type int; lexeme string}) {
	for _, token := range tokens {
		// (Type: [tokenType]; Lexeme: "[lexeme]")
		fmt.Printf("%s(%s, `%s`)%s\n", BLUE, tokenTypeToString(C.tokenType(token._type)), token.lexeme, RESET)
	}
}

// Wrapper around C.printToken using the contents of lexer.tokens
func printLexerTokens(lexer *C.Lexer) {
	fmt.Printf("%sLexer Tokens:%s\n", BLUE, RESET)
	for i := 0; i < int(lexer.tokenCount); i++ {
		token := lexerGetToken(lexer, i)
		fmt.Printf("%s(%s, `%s`)%s\n", BLUE, tokenTypeToString(token._type), fromCharPtrToGoString(token.lexeme), RESET)
	}
}