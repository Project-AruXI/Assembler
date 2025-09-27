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
	ND_SYMB,
	ND_NUMBER,
	ND_STRING,
	ND_UNKNOWN
} node_t;


typedef struct InstructionNode {

} InstrNode;

typedef struct DirectiveNode {
	// All directives either have 0 arguments (mainly section-changing directives), 1 argument, 2 arguments, or a multitude of arguments
	// The way to know which one it is is by checking the token type of the directive token

	// Maybe have all as a union???

	struct {
		struct ASTNode* data;
	} unary; // For directives with a single argument, like .string, .align, .extern, .include, .def, .glob, .zero, .sizeof

	struct {
		// All two-argument directives have a symbol as the first argument
		struct ASTNode* symb;
		struct ASTNode* data;
	} binary; // For directives with two arguments, like .size, .set, .type, .zero, .sizeof

	struct {
		struct ASTNode** exprs;
		int exprCount;
		int exprCapacity;
	} nary;
} DirctvNode;

typedef struct SymbolNode {

} SymbNode;

typedef struct NumberNode {

} NumNode;

typedef struct StringNode {

} StrNode;

typedef struct ASTNode {
	astNode_t astNodeType;

	Token* token;

	node_t nodeType;
	// The children of this node will depend on the type
	// It will be taken care of in the individual node data structures
	union {
		InstrNode* instruction;
		DirctvNode* directive;
		SymbNode* symb;
		NumNode* number;
		StrNode* string;
		void* generic;
	} nodeData;

	struct ASTNode* parent;
} Node;


Node* initASTNode(astNode_t astNodeType, node_t nodeType, Token* token, Node* parent);
void setNodeData(Node* node, void* nodeData, node_t nodeType);
void freeAST(Node* root);
void printAST(Node* root);

InstrNode* initInstructionNode();
void deinitInstructionNode(InstrNode* instrNode);




DirctvNode* initDirectiveNode();
void deinitDirectiveNode(DirctvNode* dirctvNode);

void setUnaryDirectiveData(DirctvNode* dirctvNode, Node* data);
void setBinaryDirectiveData(DirctvNode* dirctvNode, Node* symb, Node* data);
void addNaryDirectiveData(DirctvNode* dirctvNode, Node* expr);


SymbNode* initSymbolNode();
void deinitSymbolNode(SymbNode* symbNode);



NumNode* initNumberNode();
void deinitNumberNode(NumNode* numNode);



StrNode* initStringNode();
void deinitStringNode(StrNode* strNode);


#endif