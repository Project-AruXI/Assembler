#include <stdlib.h>

#include "ast.h"
#include "diagnostics.h"


Node* initASTNode(astNode_t astNodeType, node_t nodeType, Token* token, Node* parent) {
	Node* node = (Node*) malloc(sizeof(Node));
	if (!node) emitError(ERR_MEM, NULL, "Failed to allocate memory for AST node.");

	node->astNodeType = astNodeType;
	node->token = token;
	node->nodeType = nodeType;

	node->nodeData.generic = NULL;

	node->parent = parent;

	// node->children = (Node**) malloc(sizeof(Node*) * 3);
	// if (!node->children) emitError(ERR_MEM, NULL, "Failed to allocate memory for AST node children.");
	// node->childrenCapacity = 3;
	// node->childrenCount = 0;

 	return node;
}

void setNodeData(Node* node, void* nodeData, node_t nodeType) {
	switch (nodeType) {
		case ND_INSTRUCTION:
			node->nodeData.instruction = (InstrNode*) nodeData;
			break;
		case ND_DIRECTIVE:
			node->nodeData.directive = (DirctvNode*) nodeData;
			break;
		case ND_SYMB:
			node->nodeData.symb = (SymbNode*) nodeData;
			break;
		case ND_NUMBER:
			node->nodeData.number = (NumNode*) nodeData;
			break;
		case ND_STRING:
			node->nodeData.string = (StrNode*) nodeData;
			break;
		default:
			emitError(ERR_INTERNAL, NULL, "Invalid node type in setNodeData.");
			break;
	}
}

void freeAST(Node* root) {
	switch (root->nodeType) {
		case ND_INSTRUCTION:
			deinitInstructionNode(root->nodeData.instruction);
			break;
		case ND_DIRECTIVE:
			deinitDirectiveNode(root->nodeData.directive);
			break;
		case ND_SYMB:
			deinitSymbolNode(root->nodeData.symb);
			break;
		case ND_NUMBER:
			// deinitNumberNode(root->number); --- IGNORE ---
			break;
		case ND_STRING:
			// deinitStringNode(root->string); --- IGNORE ---
			break;
		default:
			emitError(ERR_INTERNAL, NULL, "Invalid node type in freeAST.");
			break;
	}

	free(root);
}

void printAST(Node* root) {
	if (!root) return;

	// Print the current node
	rlog("Node(type=%d, astNodeType=%d, token=`%s`)", root->nodeType, root->astNodeType, root->token ? root->token->lexeme : "NULL");

	// Recursively print children based on node type
	switch (root->nodeType) {
		case ND_INSTRUCTION:
			// Print instruction-specific details if needed
			break;
		case ND_DIRECTIVE: {
			DirctvNode* dirNode = root->nodeData.directive;
			if (!dirNode) break;

			if (dirNode->unary.data) {
				rlog("  Unary Directive Data:\n");
				printAST(dirNode->unary.data);
			}
			if (dirNode->binary.symb || dirNode->binary.data) {
				rlog("  Binary Directive Data:\n");
				if (dirNode->binary.symb) {
					rlog("    Symbol:\n");
					printAST(dirNode->binary.symb);
				}
				if (dirNode->binary.data) {
					rlog("    Data:\n");
					printAST(dirNode->binary.data);
				}
			}
			if (dirNode->nary.exprCount > 0) {
				rlog("  N-ary Directive Expressions:\n");
				for (int i = 0; i < dirNode->nary.exprCount; i++) {
					printAST(dirNode->nary.exprs[i]);
				}
			}
			break;
		}
		case ND_SYMB:
			// Print symbol-specific details if needed
			break;
		case ND_NUMBER:
			// Print number-specific details if needed
			break;
		case ND_STRING:
			// Print string-specific details if needed
			break;
		default:
			break;
	}
}

InstrNode* initInstructionNode() {
	return NULL;
}

void deinitInstructionNode(InstrNode* instrNode) {
}

DirctvNode* initDirectiveNode() {
	DirctvNode* node = (DirctvNode*) malloc(sizeof(DirctvNode));
	if (!node) emitError(ERR_MEM, NULL, "Failed to allocate memory for directive node.");
	
	node->unary.data = NULL;

	node->binary.symb = NULL;
	node->binary.data = NULL;

	// Initialize the array in case the directive is n-ary
	// `addNaryDirectiveData` depends that exprs is initialized
	node->nary.exprs = (Node**) malloc(sizeof(Node*) * 2);
	if (!node->nary.exprs) emitError(ERR_MEM, NULL, "Failed to allocate memory for n-ary directive expressions.");
	node->nary.exprCapacity = 2;
	node->nary.exprCount = 0;

	return node;
}

void deinitDirectiveNode(DirctvNode* dirctvNode) {
	if (!dirctvNode) return; // Only time this should happen is if the directive has no arguments at all

	if (dirctvNode->unary.data) freeAST(dirctvNode->unary.data);
	if (dirctvNode->binary.symb) freeAST(dirctvNode->binary.symb);
	if (dirctvNode->binary.data) freeAST(dirctvNode->binary.data);
	for (int i = 0; i < dirctvNode->nary.exprCount; i++) {
		freeAST(dirctvNode->nary.exprs[i]);
	}
	free(dirctvNode->nary.exprs);

	free(dirctvNode);
}

void setUnaryDirectiveData(DirctvNode* dirctvNode, Node* data) {
	dirctvNode->unary.data = data;
}

void setBinaryDirectiveData(DirctvNode* dirctvNode, Node* symb, Node* data) {
	dirctvNode->binary.symb = symb;
	dirctvNode->binary.data = data;
}

void addNaryDirectiveData(DirctvNode* dirctvNode, Node* expr) {
	if (dirctvNode->nary.exprCount == dirctvNode->nary.exprCapacity) {
		dirctvNode->nary.exprCapacity += 2;
		Node** temp = (Node**) realloc(dirctvNode->nary.exprs, sizeof(Node*) * dirctvNode->nary.exprCapacity);
		if (!temp) emitError(ERR_MEM, NULL, "Failed to reallocate memory for n-ary directive expressions.");
		dirctvNode->nary.exprs = temp;
	}
	dirctvNode->nary.exprs[dirctvNode->nary.exprCount++] = expr;
}

SymbNode* initSymbolNode() {
	return NULL;
}

void deinitSymbolNode(SymbNode* symbNode) {
}

NumNode* initNumberNode() {
	return NULL;
}

void deinitNumberNode(NumNode* numNode) {
}

StrNode* initStringNode() {
	return NULL;
}

void deinitStringNode(StrNode* strNode) {
}