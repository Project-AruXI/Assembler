#include <stdlib.h>
#include <stdbool.h>

#include "expr.h"
#include "diagnostics.h"


// Operator precedence table (higher = tighter binding)
static int getPrecedence(tokenType type) {
	switch (type) {
		case TK_ASTERISK:
		case TK_DIVIDE:
			return 6;
		case TK_PLUS:
		case TK_MINUS:
			return 5;
		case TK_BITWISE_AND:
			return 4;
		case TK_BITWISE_XOR:
			return 3;
		case TK_BITWISE_OR:
			return 2;
		default:
			return 0;
	}
}

static bool isRightAssociative(tokenType type) {
	// All are left-associative for now
	return false;
}

// Parse a primary expression: number, symbol, @, (expr)
static Node* parsePrimary(Parser* parser) {
	Token* token = parser->tokens[parser->currentTokenIndex];
	Node* node = NULL;

	switch (token->type) {
		case TK_INTEGER:
		case TK_FLOAT:
			node = initASTNode(AST_LEAF, ND_NUMBER, token, NULL);
			parser->currentTokenIndex++;
			break;
		case TK_IDENTIFIER:
		case TK_LABEL:
			node = initASTNode(AST_LEAF, ND_SYMB, token, NULL);
			parser->currentTokenIndex++;
			break;
		case TK_LP: // @
			node = initASTNode(AST_LEAF, ND_SYMB, token, NULL);
			parser->currentTokenIndex++;
			break;
		case TK_LPAREN:
			parser->currentTokenIndex++; // consume '('
			node = parseExpression(parser);
			if (parser->tokens[parser->currentTokenIndex]->type == TK_RPAREN) {
				parser->currentTokenIndex++; // consume ')'
			} else {
				linedata_ctx linedata = {
					.linenum = token->linenum,
					.source = ssGetString(token->sstring)
				};
				emitError(ERR_INVALID_SYNTAX, &linedata, "Expected ')' in expression");
			}
			break;
		default: {
			linedata_ctx linedata = {
				.linenum = token->linenum,
				.source = ssGetString(token->sstring)
			};
			emitError(ERR_INVALID_SYNTAX, &linedata, "Unexpected token in expression: %s", token->lexeme);
			break;
		}
	}
	return node;
}

// Parse a unary expression: -expr, ~expr, +expr, @expr
static Node* parseUnary(Parser* parser) {
	Token* token = parser->tokens[parser->currentTokenIndex];
	switch (token->type) {
		case TK_MINUS:
		case TK_PLUS:
		case TK_BITWISE_NOT:
		case TK_LP: // @
			parser->currentTokenIndex++;
			// Create a node for the unary operator
			Node* opNode = initASTNode(AST_INTERNAL, ND_OPERATOR, token, NULL);
			Node* operand = parseUnary(parser);
			// Attach operand as child (store in nodeData.generic for now)
			setNodeData(opNode, operand, ND_UNKNOWN);
			return opNode;
		default:
			return parsePrimary(parser);
	}
}

// Pratt/precedence climbing parser for binary operators
static Node* parseBinary(Parser* parser, int minPrec) {
	Node* left = parseUnary(parser);
	while (1) {
		Token* token = parser->tokens[parser->currentTokenIndex];
		int prec = getPrecedence(token->type);
		if (prec < minPrec) break;
		tokenType opType = token->type;
		parser->currentTokenIndex++;
		int nextMinPrec = prec + (isRightAssociative(opType) ? 0 : 1);
		Node* right = parseBinary(parser, nextMinPrec);
		// Create operator node
		Node* opNode = initASTNode(AST_INTERNAL, ND_OPERATOR, token, NULL);
		// Attach left and right as children (store as generic for now)
		Node** children = malloc(sizeof(Node*) * 2);
		children[0] = left;
		children[1] = right;
		setNodeData(opNode, children, ND_UNKNOWN);
		left = opNode;
	}
	return left;
}

Node* parseExpression(Parser* parser) {
	return parseBinary(parser, 1);
}

int evaluateExpression(Node* exprRoot, SymbolTable* symbTable) {
	return 0;
}