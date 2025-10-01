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
			node->nodeData.symbol = (SymbNode*) nodeData;
			break;
		case ND_NUMBER:
			node->nodeData.number = (NumNode*) nodeData;
			break;
		case ND_STRING:
			node->nodeData.string = (StrNode*) nodeData;
			break;
		case ND_OPERATOR:
			node->nodeData.operator = (OpNode*) nodeData;
			break;
		case ND_TYPE:
			node->nodeData.type = (TypeNode*) nodeData;
			break;
		case ND_REGISTER:
			node->nodeData.reg = (RegNode*) nodeData;
			break;
		case ND_UNKNOWN:
			node->nodeData.generic = nodeData;
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
		case ND_REGISTER:
			deinitRegisterNode(root->nodeData.reg);
			break;
		case ND_DIRECTIVE:
			deinitDirectiveNode(root->nodeData.directive);
			break;
		case ND_SYMB:
			deinitSymbolNode(root->nodeData.symbol);
			break;
		case ND_NUMBER:
			deinitNumberNode(root->nodeData.number);
			break;
		case ND_STRING:
			deinitStringNode(root->nodeData.string);
			break;
		case ND_OPERATOR:
			deinitOperatorNode(root->nodeData.operator);
			break;
		case ND_TYPE:
			deinitTypeNode(root->nodeData.type);
			break;
		case ND_UNKNOWN:
			// Nothing to free, as generic is not owned by the AST
			break;
		default:
			emitError(ERR_INTERNAL, NULL, "Invalid node type in freeAST.");
			break;
	}

	free(root);
}

void printAST(Node* root) {
	if (!root) return;

	char* nodeTypeStr = NULL;
	switch (root->nodeType) {
		case ND_INSTRUCTION:
			nodeTypeStr = "Instruction";
			break;
		case ND_REGISTER:
			nodeTypeStr = "Register";
			break;
		case ND_DIRECTIVE:
			nodeTypeStr = "Directive";
			break;
		case ND_SYMB:
			nodeTypeStr = "Symbol";
			break;
		case ND_NUMBER:
			nodeTypeStr = "Number";
			break;
		case ND_STRING:
			nodeTypeStr = "String";
			break;
		case ND_OPERATOR:
			nodeTypeStr = "Operator";
			break;
		case ND_TYPE:
			nodeTypeStr = "Type";
			break;
		default:
			nodeTypeStr = "Unknown";
			break;
	}

	char* astNodeTypeStr = NULL;
	switch (root->astNodeType) {
		case AST_LEAF:
			astNodeTypeStr = "Leaf";
			break;
		case AST_INTERNAL:
			astNodeTypeStr = "Internal";
			break;
		case AST_ROOT:
			astNodeTypeStr = "Root";
			break;
		default:
			astNodeTypeStr = "Unknown";
			break;
	}

	// Print the current node
	rlog("Node(type=%s, astNodeType=%s, token=`%s`)", nodeTypeStr, astNodeTypeStr, root->token ? root->token->lexeme : "NULL");

	// Recursively print children based on node type
	switch (root->nodeType) {
		case ND_INSTRUCTION:
			// Print instruction-specific details if needed
			break;
		case ND_REGISTER:
			// Print register-specific details if needed
			break;
		case ND_DIRECTIVE: {
			DirctvNode* dirNode = root->nodeData.directive;
			if (!dirNode) break;

			if (dirNode->unary.data) {
				rlog("  Unary Directive Data:");
				printAST(dirNode->unary.data);
			}
			if (dirNode->binary.symb || dirNode->binary.data) {
				rlog("  Binary Directive Data:");
				if (dirNode->binary.symb) {
					rlog("    Symbol:");
					printAST(dirNode->binary.symb);
				}
				if (dirNode->binary.data) {
					rlog("    Data:");
					printAST(dirNode->binary.data);
				}
			}
			if (dirNode->nary.exprCount > 0) {
				rlog("  N-ary Directive Expressions:{");
				for (int i = 0; i < dirNode->nary.exprCount; i++) {
					printAST(dirNode->nary.exprs[i]);
				}
				rlog("}");
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
		case ND_OPERATOR:
			// Print operator-specific details if needed
			break;
		case ND_TYPE:
			TypeNode* typeNode = root->nodeData.type;
			if (typeNode && typeNode->child) {
				rlog("      Type Child:");
				printAST(typeNode->child);
			}
			break;
		default:
			break;
	}
}


Node** newNodeArray(int initialCapacity) {
	Node** array = (Node**) malloc(sizeof(Node*) * initialCapacity);
	if (!array) return NULL;

	return array;
}

void freeNodeArray(Node** array) {
	free(array);
}

Node** nodeArrayInsert(Node** array, int* capacity, int* count, Node* node) {
	int cap = *capacity;
	int cnt = *count;

	if (cnt == cap) {
		cap += 2;
		Node** temp = (Node**) realloc(array, sizeof(Node*) * cap);
		if (!temp) return NULL;
		array = temp;
		*capacity = cap;
	}
	array[cnt] = node;
	*count = cnt + 1;

	return array;
}


InstrNode* initInstructionNode() {
	return NULL;
}

void deinitInstructionNode(InstrNode* instrNode) {
}

RegNode* initRegisterNode() {
	return NULL;
}

void deinitRegisterNode(RegNode* regNode) {
}

DirctvNode* initDirectiveNode() {
	DirctvNode* node = (DirctvNode*) malloc(sizeof(DirctvNode));
	if (!node) emitError(ERR_MEM, NULL, "Failed to allocate memory for directive node.");
	
	node->unary.data = NULL;

	node->binary.symb = NULL;
	node->binary.data = NULL;

	// Initialize the array in case the directive is n-ary
	// `addNaryDirectiveData` depends that exprs is initialized
	node->nary.exprs = newNodeArray(2);
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
	dirctvNode->nary.exprs = nodeArrayInsert(dirctvNode->nary.exprs, &dirctvNode->nary.exprCapacity, &dirctvNode->nary.exprCount, expr);
	if (!dirctvNode->nary.exprs) emitError(ERR_MEM, NULL, "Failed to reallocate memory for n-ary directive expressions.");
}


SymbNode* initSymbolNode(int symbTableIndex, uint32_t value) {
	SymbNode* symbNode = (SymbNode*) malloc(sizeof(SymbNode));
	if (!symbNode) emitError(ERR_MEM, NULL, "Failed to allocate memory for symbol node.");

	symbNode->symbTableIndex = symbTableIndex;
	symbNode->value = value;

	return symbNode;
}

void deinitSymbolNode(SymbNode* symbNode) {
	free(symbNode);
}


NumNode* initNumberNode(NumType type, int32_t intValue, float floatValue) {
	NumNode* numNode = (NumNode*) malloc(sizeof(NumNode));
	if (!numNode) emitError(ERR_MEM, NULL, "Failed to allocate memory for number node.");

	numNode->type = type;
	switch (type) {
		case NTYPE_INT8:
			numNode->value.int8Value = (int8_t) intValue;
			break;
		case NTYPE_INT16:
			numNode->value.int16Value = (int16_t) intValue;
			break;
		case NTYPE_INT32:
			numNode->value.int32Value = (int32_t) intValue;
			break;
		case NTYPE_FLOAT:
			numNode->value.floatValue = floatValue;
			break;
		default:
			emitError(ERR_INTERNAL, NULL, "Invalid number type in initNumberNode.");
			break;
	}

	return NULL;
}

void deinitNumberNode(NumNode* numNode) {
	free(numNode);
}


StrNode* initStringNode(sds value, int length) {
	StrNode* strNode = (StrNode*) malloc(sizeof(StrNode));
	if (!strNode) emitError(ERR_MEM, NULL, "Failed to allocate memory for string node.");

	// Since value is the lexeme itself, the surrounding quotes need to be stripped
	sds strValue = sdsnewlen(value + 1, length - 2); // +1 and -2 to skip the quotes
	if (!strValue) emitError(ERR_MEM, NULL, "Failed to allocate memory for string value.");
	strNode->value = strValue;
	strNode->length = length - 2;

	return strNode;
}

void deinitStringNode(StrNode* strNode) {
	sdsfree(strNode->value);
	free(strNode);
}


OpNode* initOperatorNode() {
	return NULL;
}

void deinitOperatorNode(OpNode* opNode) {
	// Very risky maneuver here, will free `left` and `right`, regardless it the operator is unary or binary
	// This very likely might lead to a segfault :(
	if (opNode->data.binary.left) freeAST(opNode->data.binary.left);
	if (opNode->data.binary.right) freeAST(opNode->data.binary.right);
	free(opNode);
}

TypeNode* initTypeNode() {
	TypeNode* typeNode = (TypeNode*) malloc(sizeof(TypeNode));
	if (!typeNode) emitError(ERR_MEM, NULL, "Failed to allocate memory for type node.");

	typeNode->child = NULL;
	typeNode->mainType = -1;
	typeNode->subType = -1;
	typeNode->structTableIndex = -1;

	return typeNode;
}

void deinitTypeNode(TypeNode* typeNode) {
	free(typeNode);
}

void setUnaryTypeData(TypeNode* typeNode, Node* typeData) {
	typeNode->child = typeData;
}