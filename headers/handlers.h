#ifndef _PARSER_HANDLERS_H_
#define _PARSER_HANDLERS_H_

#include "parser.h"

// Directive handlers

/**
 * Handles the `.data` directive. The directive marks the beginning of the data section.
 * @param parser The parser
 */
void handleData(Parser* parser);
/**
 * Handles the `.data` directive. The directive marks the beginning of the const section.
 * @param parser The parser
 */
void handleConst(Parser* parser);
/**
 * Handles the `.data` directive. The directive marks the beginning of the bss section.
 * @param parser The parser
 */
void handleBss(Parser* parser);
/**
 * Handles the `.data` directive. The directive marks the beginning of the text section.
 * @param parser The parser
 */
void handleText(Parser* parser);
/**
 * Handles the `.data` directive. The directive marks the beginning of the evt section.
 * @param parser The parser
 */
void handleEvt(Parser* parser);
/**
 * Handles the `.data` directive. The directive marks the beginning of the ivt section.
 * @param parser The parser
 */
void handleIvt(Parser* parser);
/**
 * Handles the `.byte` directive. The directive is used to define byte data in the current section.
 * The expressions for the directive will be parsed into ASTs and added as children to the directive's AST node.
 * @param parser The parser
 * @param directiveRoot The AST node representing the directive
 */
void handleByte(Parser* parser, Node* directiveRoot);

#endif