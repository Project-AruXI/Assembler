#ifndef _EXPR_H_
#define _EXPR_H_

#include "parser.h"
#include "ast.h"


/**
 * Builds an expression AST from the current token stream in the parser.
 * Note that this does not evaluated. It is simply parsed into an AST, which means it does not check for validity.
 * @param parser The parser
 * @return The root of the expression tree
 */
Node* parseExpression(Parser* parser);

int evaluateExpression(Node* exprRoot, SymbolTable* symbTable);

#endif