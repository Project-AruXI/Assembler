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
 * Handles the `.set` directive. The directive sets a symbol to a value.
 * If the symbol exists but not defined, it will be defined. If it is already defined, an error will be emitted.
 * Otherwise, the symbol will be created and defined.
 * @param parser The parser
 * @param directiveRoot The AST node representing the directive
 */
void handleSet(Parser* parser, Node* directiveRoot);
/**
 * Handles the `.glob` directive. The directive declares a symbol as global (external).
 * If the symbol exists but not declared as global, it will be marked as global. If it is already global, nothing will be done.
 * Otherwise, the symbol will be created and marked as global.
 * @param parser The parser
 * @param directiveRoot The AST node representing the directive
 */
void handleGlob(Parser* parser, Node* directiveRoot);

/**
 * 
 * @param parser The parser
 * @param directiveRoot The AST node representing the directive
 */
void handleString(Parser* parser, Node* directiveRoot);
/**
 * Handles the `.byte` directive. The directive is used to define byte data in the current section.
 * The expressions for the directive will be parsed into ASTs and added as children to the directive's AST node.
 * @param parser The parser
 * @param directiveRoot The AST node representing the directive
 */
void handleByte(Parser* parser, Node* directiveRoot);
void handleHword(Parser* parser, Node* directiveRoot);
void handleWord(Parser* parser, Node* directiveRoot);
void handleFloat(Parser* parser, Node* directiveRoot);
void handleZero(Parser* parser, Node* directiveRoot);
void handleFill(Parser* parser, Node* directiveRoot);

void handleSize(Parser* parser, Node* directiveRoot);
void handleType(Parser* parser, Node* directiveRoot);

void handleAlign(Parser* parser, Node* directiveRoot);
void handleExtern(Parser* parser, Node* directiveRoot);
void handleInclude(Parser* parser);
void handleDef(Parser* parser, Node* directiveRoot);
void handleSizeof(Parser* parser, Node* directiveRoot);

#endif