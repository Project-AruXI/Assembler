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

	int n = 0;

	switch (token->type) {
		case TK_IMM: // #number (immediate)
			if (token->lexeme[0] == '#') {
				// Make sure it applies to true "immediates"
				if (token->lexeme[1] == '0' && (token->lexeme[2] == 'x' || token->lexeme[2] == 'X')) {
					unsigned long u = strtoul(token->lexeme + 1, NULL, 0);
					if (u <= 0xFF) n = (int)(int8_t)u;
					else if (u <= 0xFFFF) n = (int)(int16_t)u;
					else n = (int)(int32_t)u; 
				} else if (token->lexeme[1] == '0' && (token->lexeme[2] == 'b' || token->lexeme[2] == 'B')) {
					unsigned long u = strtoul(token->lexeme + 3, NULL, 2);
					if (u <= 0xFF) n = (int)(int8_t)u;
					else if (u <= 0xFFFF) n = (int)(int16_t)u;
					else n = (int)(int32_t)u;
				} else n = atoi(token->lexeme + 1); // skip the '#'
				goto numCase;
			}
		case TK_INTEGER:
			if (token->lexeme[0] == '0' && (token->lexeme[1] == 'x' || token->lexeme[1] == 'X')) {
				unsigned long u = strtoul(token->lexeme, NULL, 0);
				if (u <= 0xFF) n = (int)(int8_t)u;
				else if (u <= 0xFFFF) n = (int)(int16_t)u;
				else n = (int)(int32_t)u; 
			} else if (token->lexeme[0] == '0' && (token->lexeme[1] == 'b' || token->lexeme[1] == 'B')) {
				unsigned long u = strtoul(token->lexeme + 2, NULL, 2);
				if (u <= 0xFF) n = (int)(int8_t)u;
				else if (u <= 0xFFFF) n = (int)(int16_t)u;
				else n = (int)(int32_t)u;
			} else n = atoi(token->lexeme);
			numCase:
			node = initASTNode(AST_LEAF, ND_NUMBER, token, NULL);
			NumType numType;
			if (n >= -128 && n <= 127) {
				rlog("Detected tyoe INT8 for number %d (0x%x)", n, n);
				numType = NTYPE_INT8;
			} else if (n >= -32768 && n <= 32767) {
				rlog("Detected tyoe INT16 for number %d (0x%x)", n, n);
				numType = NTYPE_INT16;
			} else {
				rlog("Detected tyoe INT32 for number %d (0x%x)", n, n);
				numType = NTYPE_INT32;
			}
			NumNode* numData = initNumberNode(numType, n, 0.0f);
			setNodeData(node, numData, ND_NUMBER);
			parser->currentTokenIndex++;
			break;
		case TK_FLOAT:
			node = initASTNode(AST_LEAF, ND_NUMBER, token, NULL);
			numData = initNumberNode(NTYPE_FLOAT, 0, atof(token->lexeme));
			setNodeData(node, numData, ND_NUMBER);
			parser->currentTokenIndex++;
			break;
		case TK_IDENTIFIER:
		case TK_LABEL:
			node = initASTNode(AST_LEAF, ND_SYMB, token, NULL);

			symb_entry_t* symbEntry = getSymbolEntry(parser->symbolTable, token->lexeme);
			if (!symbEntry) {
				sect_table_n sect = parser->sectionTable->activeSection;

				SYMBFLAGS flags = CREATE_FLAGS(M_NONE, T_NONE, E_EXPR, sect, L_LOC, R_REF, D_UNDEF);
				symbEntry = initSymbolEntry(token->lexeme, flags, NULL, 0, NULL, -1);
				addSymbolEntry(parser->symbolTable, symbEntry);
			}
			addSymbolReference(symbEntry, token->sstring, token->linenum);
			SET_REFERENCED(symbEntry->flags);

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
			operand->parent = opNode;
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
		left->parent = opNode;
		right->parent = opNode;
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
		// if (!exprRoot) return false;
		if (!exprRoot) emitError(ERR_INTERNAL, NULL, "Expression root node is NULL during evaluation.");

		switch (exprRoot->nodeType) {
			case ND_NUMBER: {
				// Number node: already holds value
				// Nothing to do, value is in nodeData->number
				return true;
			}
			case ND_SYMB: {
				// Symbol node: resolve from symbol table
				SymbNode* symbData = exprRoot->nodeData.symbol;
				if (!symbData) emitError(ERR_INTERNAL, NULL, "Symbol node data is NULL.");

				int idx = symbData->symbTableIndex;
				if (idx < 0 || idx >= (int)symbTable->size) emitError(ERR_INVALID_SYNTAX, NULL, "Symbol index out of bounds during expression evaluation.");

				symb_entry_t* entry = symbTable->entries[idx];
				if (!entry) emitError(ERR_INTERNAL, NULL, "Symbol entry is NULL during expression evaluation.");

				detail("%s: flags: 0x%x; expr: %p", entry->name, entry->flags, (void*)entry->value.expr);

				// If symbol is defined as value, propagate
				if (GET_DEFINED(entry->flags) && GET_EXPRESSION(entry->flags) == E_VAL) {
					symbData->value = entry->value.val;
					
					// Get the smallest type
					if (entry->value.val <= 0xFF) symbData->type = NTYPE_INT8;
					else if (entry->value.val <= 0xFFFF) symbData->type = NTYPE_INT16;
					else symbData->type = NTYPE_INT32;

					return true;
				} else if (GET_EXPRESSION(entry->flags) == E_EXPR && entry->value.expr) {
					// Evaluate the symbol's expression
					if (evaluateExpression(entry->value.expr, symbTable)) {
						// After evaluation, update symbol value
						// Assume result is in root node of entry->value.expr
						Node* resNode = entry->value.expr;

						NumType resType;
						uint32_t resValue;

						if (resNode->nodeType == ND_NUMBER) {
							NumNode* numData = resNode->nodeData.number;
							resType = numData->type;
							switch (resType) {
								case NTYPE_INT8: resValue = (uint32_t) numData->value.int8Value; break;
								case NTYPE_INT16: resValue = (uint32_t) numData->value.int16Value; break;
								case NTYPE_INT32: resValue = (uint32_t) numData->value.int32Value; break;
								case NTYPE_UINT32: resValue = numData->value.uint32Value; break;
								case NTYPE_FLOAT: emitError(ERR_INVALID_EXPRESSION, NULL, "Symbol expression evaluated to float, which is not supported for symbol values.");
								default: emitError(ERR_INTERNAL, NULL, "Unknown numeric type in symbol expression evaluation.");
							}
						} else if (resNode->nodeType == ND_SYMB) {
							SymbNode* resSymb = resNode->nodeData.symbol;
							resValue = resSymb->value;
							resType = resSymb->type;
						} else if (resNode->nodeType == ND_OPERATOR) {
							OpNode* resOp = resNode->nodeData.operator;
							resValue = resOp->value;
							resType = resOp->valueType;
						} else emitError(ERR_INTERNAL, NULL, "Unexpected node type in symbol expression evaluation.");

						symbData->value = resValue;
						symbData->type = resType;

						// Optionally update entry to hold value
						// updateSymbolEntry(entry, SET_DEFINED(CLR_EXPRESSION(entry->flags)), symbData->value);
						entry->value.expr = NULL;
						entry->value.val = symbData->value;
						SET_DEFINED(entry->flags);
						CLR_EXPRESSION(entry->flags);
						return true;
					}
				}
				// Symbol not defined
				return false;
			}
			case ND_OPERATOR: {
				OpNode* opData = (OpNode*)exprRoot->nodeData.operator;
				if (!opData) emitError(ERR_INTERNAL, NULL, "Operator node data is NULL.");
				// Determine if unary or binary
				Node *left = NULL, *right = NULL;
				if (opData->data.binary.left && opData->data.binary.right) {
					left = opData->data.binary.left;
					right = opData->data.binary.right;
					if (!evaluateExpression(left, symbTable)) return false;
					if (!evaluateExpression(right, symbTable)) return false;
				} else if (opData->data.unary.operand) {
					left = opData->data.unary.operand;
					if (!evaluateExpression(left, symbTable)) return false;
				} else {
					emitError(ERR_INTERNAL, NULL, "Operator node has neither unary nor binary operands.");
				}
				// Get operand values
				// Type promotion and smallest type selection
				NumType ltype = NTYPE_INT32, rtype = NTYPE_INT32, resultType = NTYPE_INT32;
				int64_t lval = 0, rval = 0, result = 0;
				float lfval = 0.0f, rfval = 0.0f, fres = 0.0f;
				bool isFloat = false;
				if (left && left->nodeType == ND_NUMBER) {
					NumNode* lnum = (NumNode*)left->nodeData.number;
					ltype = lnum->type;
					switch (ltype) {
						case NTYPE_FLOAT: lfval = lnum->value.floatValue; isFloat = true; break;
						case NTYPE_INT8: lval = lnum->value.int8Value; break;
						case NTYPE_INT16: lval = lnum->value.int16Value; break;
						case NTYPE_INT32: lval = lnum->value.int32Value; break;
						case NTYPE_UINT32: lval = lnum->value.int32Value; break;
						default: lval = lnum->value.int32Value; break;
					}
				} else if (left && left->nodeType == ND_SYMB) {
					SymbNode* lsymb = (SymbNode*)left->nodeData.symbol;
					lval = lsymb->value;
					ltype = lsymb->type;
				}
				if (right && right->nodeType == ND_NUMBER) {
					NumNode* rnum = (NumNode*)right->nodeData.number;
					rtype = rnum->type;
					switch (rtype) {
						case NTYPE_FLOAT: rfval = rnum->value.floatValue; isFloat = true; break;
						case NTYPE_INT8: rval = rnum->value.int8Value; break;
						case NTYPE_INT16: rval = rnum->value.int16Value; break;
						case NTYPE_INT32: rval = rnum->value.int32Value; break;
						case NTYPE_UINT32: rval = rnum->value.int32Value; break;
						default: rval = rnum->value.int32Value; break;
					}
				} else if (right && right->nodeType == ND_SYMB) {
					SymbNode* rsymb = (SymbNode*)right->nodeData.symbol;
					rval = rsymb->value;
					rtype = rsymb->type;
				}
				// Promote type if needed
				if (isFloat || ltype == NTYPE_FLOAT || rtype == NTYPE_FLOAT) {
					resultType = NTYPE_FLOAT;
				} else if (ltype > rtype) {
					resultType = ltype;
				} else {
					resultType = rtype;
				}
				// Apply operator
				if (resultType == NTYPE_FLOAT) {
					switch (exprRoot->token->type) {
						case TK_PLUS: fres = lfval + rfval; break;
						case TK_MINUS:
							if (right) fres = lfval - rfval;
							else fres = -lfval;
							break;
						case TK_ASTERISK: fres = lfval * rfval; break;
						case TK_DIVIDE: fres = rfval ? lfval / rfval : 0.0f; break;
						default: emitError(ERR_INVALID_EXPRESSION, NULL, "Invalid operator for float expression.");
					}
					opData->valueType = NTYPE_FLOAT;
					opData->value = (uint32_t) fres;
				} else {
					switch (exprRoot->token->type) {
						case TK_PLUS: result = lval + rval; break;
						case TK_MINUS:
							if (right) result = lval - rval;
							else result = -lval;
							break;
						case TK_ASTERISK: result = lval * rval; break;
						case TK_DIVIDE: result = rval ? lval / rval : 0; break;
						case TK_BITWISE_AND: result = lval & rval; break;
						case TK_BITWISE_OR: result = lval | rval; break;
						case TK_BITWISE_XOR: result = lval ^ rval; break;
						case TK_BITWISE_NOT: result = ~lval; break;
						case TK_BITWISE_SL: result = lval << rval; break;
						case TK_BITWISE_SR: result = lval >> rval; break;
						default: emitError(ERR_INVALID_EXPRESSION, NULL, "Invalid operator for integer expression.");
					}
					opData->valueType = resultType;
					opData->value = (uint32_t)result;
				}
				// Propagate result to root node
				// Optionally, set the result in a new NumNode at the root if needed
				return true;
			}
			default: emitError(ERR_INTERNAL, NULL, "Invalid node type in expression evaluation.");
		}
		return false;
}

Node* getExternSymbol(Node* exprRoot) { // TODO: change name, better indicate if invalid expr or otherwise
	if (exprRoot->nodeType != ND_OPERATOR) {
		// If no operator, it means it is just the symbol itself
		if (exprRoot->nodeType == ND_SYMB) return exprRoot;
		else return NULL;
	}

	OpNode* opData = (OpNode*)exprRoot->nodeData.operator;
	if (!opData) return NULL;

	Node* left = opData->data.binary.left;
	Node* right = opData->data.binary.right;
	if (!left || !right) return NULL;
	// This makes sure that the only expression form is `symb|number +/- number|symb`
	if (exprRoot->token->type != TK_PLUS && exprRoot->token->type != TK_MINUS) return NULL;

	if (left->nodeType == ND_SYMB && right->nodeType == ND_NUMBER) return left;
	else if (left->nodeType == ND_NUMBER && right->nodeType == ND_SYMB) return right;
	else return NULL;

	return NULL;
}