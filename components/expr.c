#include <stdlib.h>
#include <stdbool.h>

#include "expr.h"
#include "diagnostics.h"




// Forward declarations
static Node* parsePrimary(Parser* parser);
static Node* parseUnary(Parser* parser);
static Node* parseBinary(Parser* parser, int minPrec);
static int getPrecedence(tokenType type);
static bool isRightAssociative(tokenType type);

// Operator precedence table (higher = tighter binding)
static int getPrecedence(tokenType type) {
	switch (type) {
		case TK_ASTERISK:
		case TK_DIVIDE:
			return 6; // * /
		case TK_PLUS:
		case TK_MINUS:
			return 5; // + -
		case TK_BITWISE_SL: // <<
		case TK_BITWISE_SR: // >>
			return 4;
		case TK_BITWISE_AND:
			return 3; // &
		case TK_BITWISE_XOR:
			return 2; // ^
		case TK_BITWISE_OR:
			return 1; // |
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
	initScope("parsePrimary");
	Token* token = parser->tokens[parser->currentTokenIndex];
	Node* node = NULL;

	switch (token->type) {
		case TK_IMM: // #number (immediate)
		case TK_INTEGER:
		case TK_FLOAT:
			node = initASTNode(AST_LEAF, ND_NUMBER, token, NULL);
			NumNode* numData = initNumberNode(NTYPE_INT32, 0, 0.0f);
			setNodeData(node, numData, ND_NUMBER);
			parser->currentTokenIndex++;
			break;
		case TK_IDENTIFIER:
		case TK_LABEL:
			node = initASTNode(AST_LEAF, ND_SYMB, token, NULL);

			symb_entry_t* symbEntry = getSymbolEntry(parser->symbolTable, token->lexeme);
			if (!symbEntry) {
				SYMBFLAGS flags = CREATE_FLAGS(M_NONE, T_NONE, E_EXPR, S_UNDEF, L_LOC, R_REF, D_UNDEF);
				symbEntry = initSymbolEntry(token->lexeme, flags, NULL, 0, NULL, -1);
				addSymbolEntry(parser->symbolTable, symbEntry);
			}
			addSymbolReference(symbEntry, token->sstring, token->linenum);

			SymbNode* symbData = initSymbolNode(symbEntry->symbTableIndex, 0);
			setNodeData(node, symbData, ND_SYMB);
			parser->currentTokenIndex++;
			break;
		case TK_LP: // @
			// Need to solidify the actual
			node = initASTNode(AST_LEAF, ND_NUMBER, token, NULL);
			numData = initNumberNode(NTYPE_UINT32, parser->sectionTable->entries[parser->sectionTable->activeSection].lp, 0.0f);
			setNodeData(node, numData, ND_NUMBER);
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

// Parse a unary expression: -expr, ~expr, +expr
static Node* parseUnary(Parser* parser) {
	initScope("parseUnary");

	Token* token = parser->tokens[parser->currentTokenIndex];
	switch (token->type) {
		case TK_MINUS:
		case TK_PLUS:
		case TK_BITWISE_NOT:
			parser->currentTokenIndex++;
			// Create a node for the unary operator
			OpNode* opData = initOperatorNode();
			Node* opNode = initASTNode(AST_INTERNAL, ND_OPERATOR, token, NULL);
			Node* operand = parseUnary(parser);
			setNodeData(opNode, opData, ND_OPERATOR);
			setUnaryOperand(opData, operand);

			return opNode;
		default:
			return parsePrimary(parser);
	}
}

// Pratt/precedence climbing parser for binary operators
static Node* parseBinary(Parser* parser, int minPrec) {
	initScope("parseBinary");

	Node* left = parseUnary(parser);
	while (true) {
		Token* token = parser->tokens[parser->currentTokenIndex];
		int prec = getPrecedence(token->type);
		if (prec < minPrec || prec == 0) break;
		tokenType opType = token->type;
		parser->currentTokenIndex++;
		int nextMinPrec = prec + (isRightAssociative(opType) ? 0 : 1);
		Node* right = parseBinary(parser, nextMinPrec);
		Node* opNode = initASTNode(AST_INTERNAL, ND_OPERATOR, token, NULL);
		OpNode* opData = initOperatorNode();
		setNodeData(opNode, opData, ND_OPERATOR);
		setBinaryOperands(opData, left, right);
		left = opNode;
	}
	return left;
}

// Parse an expression (entry point)
Node* parseExpression(Parser* parser) {
	// Save the starting token index
	int startIdx = parser->currentTokenIndex;
	Node* expr = parseBinary(parser, 1);

	// Check if this is a single-token expression
	int endIdx = parser->currentTokenIndex;
	if (endIdx - startIdx == 1) {
		Token* tok = parser->tokens[startIdx];
		if (tok->type == TK_INTEGER || tok->type == TK_FLOAT) {
			linedata_ctx linedata = {
				.linenum = tok->linenum,
				.source = ssGetString(tok->sstring)
			};
			emitError(ERR_INVALID_SYNTAX, &linedata, "A single-number expression must use '#' (immediate), not a plain number.");
		}
	}
	return expr;
}

bool evaluateExpression(Node* exprRoot, SymbolTable* symbTable) {
	return 0;
}