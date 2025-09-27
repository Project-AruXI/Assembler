#include <stdlib.h>

#include "ast.h"
#include "diagnostics.h"


Node* initASTNode(astNode_t astNodeType, node_t nodeType, Token* token, Node* parent) {
	Node* node = (Node*) malloc(sizeof(Node));
	if (!node) emitError(ERR_MEM, NULL, "Failed to allocate memory for AST node.");

	node->astNodeType = astNodeType;
	node->token = token;
	node->nodeType = nodeType;

	node->parent = parent;

	node->children = (Node**) malloc(sizeof(Node*) * 3);
	if (!node->children) emitError(ERR_MEM, NULL, "Failed to allocate memory for AST node children.");
	node->childrenCapacity = 3;
	node->childrenCount = 0;

 	return node;
}

void addChild(Node* parent, Node* child) {
	if (parent->childrenCount >= parent->childrenCapacity) {
		parent->childrenCapacity += 2;
		parent->children = (Node**) realloc(parent->children, sizeof(Node*) * parent->childrenCapacity);
		if (!parent->children) emitError(ERR_MEM, NULL, "Failed to reallocate memory for AST node children.");
	}
	parent->children[parent->childrenCount++] = child;
	child->parent = parent;
}

void freeAST(Node* root) {
	if (!root) return;

	for (int i = 0; i < root->childrenCount; i++) {
		freeAST(root->children[i]);
	}
	free(root->children);

	// Free specific node data based on type
	switch (root->nodeType) {
		case ND_INSTRUCTION:
			free(root->instruction);
			break;
		case ND_DIRECTIVE:
			free(root->directive);
			break;
		default:
			break;
	}

	free(root);
}