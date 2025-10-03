#ifndef _EXPR_H_
#define _EXPR_H_

#include "parser.h"
#include "ast.h"


/**
 * Builds an expression AST from the current token stream in the parser.
 * Note that this does not evaluate. It is simply parsed into an AST, 
 *   which means it does not check for validity in terms of result outcome.
 * It does, however, check that it is a well-formed expression.
 * @param parser The parser
 * @return The root of the expression tree
 */
Node* parseExpression(Parser* parser);

/**
 * Evaluates the expression tree that starts with `exprRoot`, placing the resulting value in its value field.
 * @param exprRoot The root of the expression tree
 * @param symbTable The symbol table
 * @return True if evaluation was successful, false if not
 */
bool evaluateExpression(Node* exprRoot, SymbolTable* symbTable);

#endif