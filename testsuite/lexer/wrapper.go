package lexerTests

/*
#cgo LDFLAGS: -L../../out -llexer
#include <stdlib.h>
#include "../../headers/lexer.h"
#include "../../headers/diagnostics.h"
#include "../../headers/token.h"
*/
import "C"
import "unsafe"


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


const (
	RESET = "\033[0m"
	RED = "\033[31m"
	GREEN = "\033[32m"
	YELLOW = "\033[33m"
)