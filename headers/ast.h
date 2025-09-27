#ifndef _AST_H_
#define _AST_H_

#include "token.h"


typedef enum {
	AST_LEAF,
	AST_INTERNAL,
	AST_ROOT
} astNode_t;


typedef enum {
	ND_INSTRUCTION,
	ND_DIRECTIVE,
	ND_LABEL,
	ND_SYMB,
	// ND_EXPRESSION,
	// ND_STATEMENT,
	// ND_MACRO,
	// ND_MACRO_CALL,
	// ND_CONDITIONAL,
	// ND_TYPE_DEF,
	// ND_VARIABLE_DEF,
	// ND_FUNCTION_DEF,
	// ND_PARAMETER,
	// ND_ARGUMENT,
	// ND_BLOCK,
	ND_UNKNOWN
} node_t;


typedef struct InstructionNode {

} InstrNode;

typedef struct DirectiveNode {

} DirctvNode;



typedef struct ASTNode {
	astNode_t astNodeType;

	Token* token;

	node_t nodeType;
	union {
		InstrNode* instruction;
		DirctvNode* directive;
	};


	struct ASTNode* parent;
	struct ASTNode** children;
	int childrenCount;
	int childrenCapacity;
} Node;


Node* initASTNode(astNode_t astNodeType, node_t nodeType, Token* token, Node* parent);
void addChild(Node* parent, Node* child);
void freeAST(Node* root);


InstrNode* initInstructionNode();


#endif