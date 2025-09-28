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
	ND_REGISTER,
	ND_DIRECTIVE,
	ND_SYMB,
	ND_NUMBER,
	ND_STRING,
	ND_OPERATOR,
	ND_TYPE,
	ND_UNKNOWN
} node_t;


typedef struct InstructionNode {

} InstrNode;

typedef struct RegisterNode {
	int regNumber; // The register number, ie, for x0, the number is 0, or for sp, it is 31
	// Might need more info????
} RegNode;

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
	// The name of the symbol is in the token's lexeme, no need to have it here
	int symbTableIndex; // The index of the symbol in the symbol table, -1 if not yet added
	// The symbol's value is in the symbol table entry, but maybe have it???
	uint32_t value;
} SymbNode;

typedef enum {
	NTYPE_INT8,
	NTYPE_INT16,
	NTYPE_INT32,
	NTYPE_FLOAT
} NumType;

typedef struct NumberNode {
	union {
		int8_t int8Value;
		int16_t int16Value;
		int32_t int32Value;
		float floatValue;
	} value;
	NumType type;
} NumNode;

typedef struct StringNode {
	// Note that the string value is not the same as the lexeme in the token
	// The lexeme includes the surrounding quotes
	// This contains the actual string value
	// This means that `value` is to be allocated and freed
	sds value;
	int length;
} StrNode;

typedef struct OperatorNode {
	// An operand can be either unary or binary at a time
	// For unary, only `operand` is used
	// For binary, both `left` and `right` are used
	union {
		struct {
			struct ASTNode* operand;
		} unary;
		struct {
			struct ASTNode* left;
			struct ASTNode* right;
		} binary;
	} data;

	// During expression evaluation, the operator will hold the computed value of the subtree
	// Note that the operation result types are not explicitly differentiated here
	// However, the type will be stored due to backtracking
	uint32_t value;
	NumType valueType;
} OpNode;

typedef struct TypeNode {
	// The type being specified by the .type directive
	enum Type {
		// Main types
		TYPE_FUNC,
		TYPE_OBJECT,

		// Sub types
		TYPE_ARRAY,
		TYPE_STRUCT,
		TYPE_UNION,
		TYPE_PTR
	};

	union {
		enum Type mainType;
		enum Type subType;
		int structTableIndex; // If subType is TYPE_STRUCT, the index of the struct in the struct table
	};
	// Know what type it is will be inferred from context/going throught the AST
	// Since if the previous AST Node is a .type directive, then this is a main type
	// And thus if the previous is a main type, this is a subtype
	// Finally if the previous is a subtype, this is a tag type
} TypeNode;

typedef struct ASTNode {
	astNode_t astNodeType;

	Token* token;

	node_t nodeType;
	// The children of this node will depend on the type
	// It will be taken care of in the individual node data structures
	union {
		InstrNode* instruction;
		RegNode* reg;
		DirctvNode* directive;
		SymbNode* symbol;
		NumNode* number;
		StrNode* string;
		OpNode* operator;
		TypeNode* type;
		void* generic;
	} nodeData;

	struct ASTNode* parent;
} Node;


Node* initASTNode(astNode_t astNodeType, node_t nodeType, Token* token, Node* parent);
void setNodeData(Node* node, void* nodeData, node_t nodeType);
void freeAST(Node* root);
void printAST(Node* root);

// Dynamic array functionality

/**
 * Creates a new dynamic array of AST nodes with the given initial capacity.
 * @param initialCapacity The initial capacity of the array
 * @return The new array of AST nodes or NULL if memory allocation failed
 */
Node** newNodeArray(int initialCapacity);
/**
 * Frees the given array of nodes. Note that this does not free the nodes themselves as they are owned by the parser.
 * @param array The array of nodes to free
 */
void freeNodeArray(Node** array);

/**
 * Inserts a node into the array, resizing if necessary.
 * @param array The array of nodes
 * @param capacity The capacity of the array, updated if resized
 * @param count The current count of nodes in the array, updated after insertion
 * @param node The node to add
 * @return The (possibly reallocated) array of nodes or NULL if memory reallocation failed
 */
Node** nodeArrayInsert(Node** array, int* capacity, int* count, Node* node);


InstrNode* initInstructionNode();
void deinitInstructionNode(InstrNode* instrNode);




RegNode* initRegisterNode();
void deinitRegisterNode(RegNode* regNode);




DirctvNode* initDirectiveNode();
void deinitDirectiveNode(DirctvNode* dirctvNode);

void setUnaryDirectiveData(DirctvNode* dirctvNode, Node* data);
void setBinaryDirectiveData(DirctvNode* dirctvNode, Node* symb, Node* data);
void addNaryDirectiveData(DirctvNode* dirctvNode, Node* expr);


SymbNode* initSymbolNode(int symbTableIndex, uint32_t value);
void deinitSymbolNode(SymbNode* symbNode);




NumNode* initNumberNode(NumType type, int32_t intValue, float floatValue);
void deinitNumberNode(NumNode* numNode);




StrNode* initStringNode(sds value, int length);
void deinitStringNode(StrNode* strNode);




OpNode* initOperatorNode();
void deinitOperatorNode(OpNode* opNode);




TypeNode* initTypeNode();
void deinitTypeNode(TypeNode* typeNode);





#endif