#include <ctype.h>

#include "handlers.h"
#include "ast.h"
#include "diagnostics.h"
#include "SymbolTable.h"


static int normalizeRegister(const char* regStr) {
	// Some registers have aliases, for example x0, a0, and xr are the same register (Register 0)
	// This is where it converts them to their corresponding number
	// In the case that regStr does not correspond to a valid register, return -1

	struct {
		const char* name;
		int number;
	} regAliases[] = {
		{"xr", 0}, {"a0", 0}, {"x0", 0},
		{"a1", 1}, {"x1", 1},
		{"a2", 2}, {"x2", 2},
		{"a3", 3}, {"x3", 3},
		{"a4", 4}, {"x4", 4},
		{"a5", 5}, {"x5", 5},
		{"a6", 6}, {"x6", 6},
		{"a7", 7}, {"x7", 7},
		{"a8", 8}, {"x8", 8},
		{"a9", 9}, {"x9", 9},
		{"x10", 10},
		{"x11", 11},
		{"c0", 12}, {"x12", 12},
		{"c1", 13}, {"x13", 13},
		{"c2", 14}, {"x14", 14},
		{"c3", 15}, {"x15", 15},
		{"c4", 16}, {"x16", 16},
		{"s0", 17}, {"x17", 17},
		{"s1", 18}, {"x18", 18},
		{"s2", 19}, {"x19", 19},
		{"s3", 20}, {"x20", 20},
		{"s4", 21}, {"x21", 21},
		{"s5", 22}, {"x22", 22},
		{"s6", 23}, {"x23", 23},
		{"s7", 24}, {"x24", 24},
		{"s8", 25}, {"x25", 25},
		{"s9", 26}, {"x26", 26},
		{"s10", 27}, {"x27", 27},
		{"lr", 28}, {"x28", 28},
		{"xb", 29}, {"x29", 29},
		{"xz", 30}, {"x30", 30},
		{"sp", 31}
	};
	int n = sizeof(regAliases) / sizeof(regAliases[0]);
	for (int i = 0; i < n; ++i) {
		if (strcasecmp(regStr, regAliases[i].name) == 0) return regAliases[i].number;
	}

	return -1;
}

static void validateSymbolToken(Token* token, linedata_ctx* linedata) {
	// Make sure the symbol is valid
	if (token->lexeme[0] != '_' && !isalpha(token->lexeme[0])) {
		emitError(ERR_INVALID_LABEL, linedata, "Symbol must start with an alphabetic character or underscore: `%s`", token->lexeme);
	}

	// Also make sure the symbol is not a reserved word
	if (indexOf(REGISTERS, sizeof(REGISTERS)/sizeof(REGISTERS[0]), token->lexeme) != -1 ||
			indexOf(DIRECTIVES, sizeof(DIRECTIVES)/sizeof(DIRECTIVES[0]), token->lexeme) != -1 ||
			indexOf(INSTRUCTIONS, sizeof(INSTRUCTIONS)/sizeof(INSTRUCTIONS[0]), token->lexeme) != -1) {
		emitError(ERR_INVALID_LABEL, linedata, "Symbol cannot be a reserved word: `%s`", token->lexeme);
	}
}

void handleIR(Parser* parser, Node* instrRoot) {
	initScope("handleIR");

	Token* instrToken = parser->tokens[parser->currentTokenIndex];
	linedata_ctx linedata = {
		.linenum = instrToken->linenum,
		.source = ssGetString(instrToken->sstring)
	};

	log("Handling IR instruction at line %d", instrToken->linenum);

	enum Instructions instrType = instrRoot->nodeData.instruction->instruction;

	// Instructions can be both I-type or R-type depending on the operands
	// `mv x0, imm` is I-type, but `mv x0, x1` is R-type
	// Operands will determine the type
	// Also taking into consideration that some instructions, regardless of type, have two or three operands

	parser->currentTokenIndex++;
	Token* nextToken = parser->tokens[parser->currentTokenIndex];
	// This token better be a register (xd to be specific if the instruction is not cmp, cmp has xs as its first)
	if (nextToken->type != TK_REGISTER) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected a register as the first operand of `%s` instruction, got `%s`.", instrToken->lexeme, nextToken->lexeme);
	int regNum = normalizeRegister(nextToken->lexeme);
	if (regNum == -1) emitError(ERR_INVALID_REGISTER, &linedata, "Invalid register: `%s`.", nextToken->lexeme);

	Node* xdNode = initASTNode(AST_LEAF, ND_REGISTER, nextToken, instrRoot);
	RegNode* xdData = initRegisterNode(regNum);
	setNodeData(xdNode, xdData, ND_REGISTER);

	Node* xsNode = NULL;
	Node* xrNode = NULL;

	// cmp is special, its first operand is xs
	if (instrType == CMP) {
		xsNode = xdNode;
		xdNode = NULL;
	}

	// Need to attach xdNode to instrRoot as a children
	// However, this depends on what type of instr, will do later

	parser->currentTokenIndex++;
	nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type != TK_COMMA) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected `,` after first operand of `%s` instruction, got `%s`.", instrToken->lexeme, nextToken->lexeme);

	parser->currentTokenIndex++;
	nextToken = parser->tokens[parser->currentTokenIndex];

	// The following token can have some variation
	// It either can be:
	// - An immediate (in this case, the instruction is I-type with two operands `xd, imm` or `xs, imm` for cmp)
	//     This means that there can be no more tokens
	// - A register followed by another register (definitive R-type with three operands)
	// - A register followed by a newline (definity R-type with two operands)
	// - A register followed by an immediate (definitive I-type)
	
	if (nextToken->type == TK_IMM) {
		// Immediate, so I-type with two operands
		Node* immNode = initASTNode(AST_LEAF, ND_NUMBER, nextToken, instrRoot);
		NumNode* immData = initNumberNode(NTYPE_UINT14, atoi(nextToken->lexeme + 1), 0.0);
		setNodeData(immNode, immData, ND_NUMBER);

		// Set the instruction data
		if (instrType == CMP) {
			// Temporarily directly set, need to use a function later
			instrRoot->nodeData.instruction->data.iType.xs = xsNode;
			instrRoot->nodeData.instruction->data.iType.imm = immNode;
		} else {
			instrRoot->nodeData.instruction->data.iType.xd = xdNode;
			instrRoot->nodeData.instruction->data.iType.imm = immNode;
		}

		parser->currentTokenIndex++;
		nextToken = parser->tokens[parser->currentTokenIndex];
		if (nextToken->type != TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected newline after immediate operand of `%s` instruction, got `%s`.", instrToken->lexeme, nextToken->lexeme);
		
		parser->currentTokenIndex++; // Consume the newline

		return;
	} else if (nextToken->type == TK_REGISTER) {
		// Register, so either R-type (xd, xs, xr or xs, xr) or I-type (xd, xs, imm)
		regNum = normalizeRegister(nextToken->lexeme);
		if (regNum == -1) emitError(ERR_INVALID_REGISTER, &linedata, "Invalid register: `%s`.", nextToken->lexeme);

		Node* cmpXsNode = xsNode; // Save for cmp (reg), the xs of the other R-types is the xr of cmp
		xsNode = initASTNode(AST_LEAF, ND_REGISTER, nextToken, instrRoot); // xs node or xr for cmp
		RegNode* xsData = initRegisterNode(regNum);
		// Need to set the appropriate node
		if (instrType == CMP) {
			// Not necessary??? but allows a distinction
			xrNode = xsNode;
			RegNode* xrData = xsData;
			setNodeData(xrNode, xrData, ND_REGISTER);
			xsNode = cmpXsNode;
		} else setNodeData(xsNode, xsData, ND_REGISTER);

		parser->currentTokenIndex++;
		nextToken = parser->tokens[parser->currentTokenIndex];
		if (nextToken->type == TK_NEWLINE) {
			// Definitive R-type with two operands
			if (instrType == CMP) {
				// Temporarily directly set, need to use a function later
				instrRoot->nodeData.instruction->data.rType.xs = xsNode;
				instrRoot->nodeData.instruction->data.rType.xr = xrNode;
			} else {
				instrRoot->nodeData.instruction->data.rType.xd = xdNode;
				instrRoot->nodeData.instruction->data.rType.xs = xsNode;
			}

			parser->currentTokenIndex++; // Consume the newline

			return;
		} else if (nextToken->type == TK_COMMA) {
			// Could be either R-type with three operands or I-type with three operands
			parser->currentTokenIndex++;
			nextToken = parser->tokens[parser->currentTokenIndex];

			if (nextToken->type == TK_REGISTER) {
				// Definitive R-type with three operands
				regNum = normalizeRegister(nextToken->lexeme);
				if (regNum == -1) emitError(ERR_INVALID_REGISTER, &linedata, "Invalid register: `%s`.", nextToken->lexeme);

				xrNode = initASTNode(AST_LEAF, ND_REGISTER, nextToken, instrRoot);
				RegNode* xrData = initRegisterNode(regNum);
				setNodeData(xrNode, xrData, ND_REGISTER);

				instrRoot->nodeData.instruction->data.rType.xd = xdNode;
				instrRoot->nodeData.instruction->data.rType.xs = xsNode;
				instrRoot->nodeData.instruction->data.rType.xr = xrNode;

				parser->currentTokenIndex++;
				nextToken = parser->tokens[parser->currentTokenIndex];
				if (nextToken->type != TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected newline after third operand of `%s` instruction, got `%s`.", instrToken->lexeme, nextToken->lexeme);
				parser->currentTokenIndex++; // Consume the newline

				return;
			} else if (nextToken->type == TK_IMM) {
				// Definitive I-type with three operands
				Node* immNode = initASTNode(AST_LEAF, ND_NUMBER, nextToken, instrRoot);
				NumNode* immData = initNumberNode(NTYPE_UINT14, atoi(nextToken->lexeme + 1), 0.0);
				setNodeData(immNode, immData, ND_NUMBER);

				instrRoot->nodeData.instruction->data.iType.xd = xdNode;
				instrRoot->nodeData.instruction->data.iType.xs = xsNode;
				instrRoot->nodeData.instruction->data.iType.imm = immNode;

				parser->currentTokenIndex++;
				nextToken = parser->tokens[parser->currentTokenIndex];
				if (nextToken->type != TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected newline after immediate operand of `%s` instruction, got `%s`.", instrToken->lexeme, nextToken->lexeme);
				parser->currentTokenIndex++; // Consume the newline

				return;
			}

			emitError(ERR_INVALID_SYNTAX, &linedata, "Expected a register or an immediate as the third operand of `%s` instruction, got `%s`.", instrToken->lexeme, nextToken->lexeme);
		}

		emitError(ERR_INVALID_SYNTAX, &linedata, "Expected `,` or newline after second operand of `%s` instruction, got `%s`.", instrToken->lexeme, nextToken->lexeme);
	}

	emitError(ERR_INVALID_SYNTAX, &linedata, "Expected a register or an immediate as the second operand of `%s` instruction, got `%s`.", instrToken->lexeme, nextToken->lexeme);
}

void handleI(Parser* parser, Node* instrRoot) {
	initScope("handleI");

	Token* instrToken = parser->tokens[parser->currentTokenIndex];
	linedata_ctx linedata = {
		.linenum = instrToken->linenum,
		.source = ssGetString(instrToken->sstring)
	};

	log("Handling I instruction at line %d", instrToken->linenum);

	enum Instructions instrType = instrRoot->nodeData.instruction->instruction;

	// Instructions are in `xd, xs, imm` format except nop where it is an alias of `add xz, xz, #0`
	// However, as of now, the only true I-type (that does not have an R-type form) is `nop`
	// So just handle nop
	// For sure, when ISA is expanded, this will change

	if (instrType != NOP) emitError(ERR_INTERNAL, &linedata, "Current ISA dictates NOP to be the only I-type instruction. Got `%s`.", instrToken->lexeme);

	// Make sure nothing after

	parser->currentTokenIndex++;
	Token* nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type != TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected newline after `%s` instruction, got `%s`.", instrToken->lexeme, nextToken->lexeme);
	parser->currentTokenIndex++; // Consume the newline
}

void handleR(Parser* parser, Node* instrRoot) {
	initScope("handleR");

	Token* instrToken = parser->tokens[parser->currentTokenIndex];
	linedata_ctx linedata = {
		.linenum = instrToken->linenum,
		.source = ssGetString(instrToken->sstring)
	};

	log("Handling R instruction at line %d", instrToken->linenum);

	// enum Instructions instrType = instrRoot->nodeData.instruction->instruction;

	// Instructions are in `xd, xs, xr` form

	parser->currentTokenIndex++;
	Token* nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type != TK_REGISTER) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected a register as the first operand of `%s` instruction, got `%s`.", instrToken->lexeme, nextToken->lexeme);
	int regNum = normalizeRegister(nextToken->lexeme);
	if (regNum == -1) emitError(ERR_INVALID_REGISTER, &linedata, "Invalid register: `%s`.", nextToken->lexeme);

	Node* xdNode = initASTNode(AST_LEAF, ND_REGISTER, nextToken, instrRoot);
	RegNode* xdData = initRegisterNode(regNum);
	setNodeData(xdNode, xdData, ND_REGISTER);

	parser->currentTokenIndex++;
	nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type != TK_COMMA) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected `,` after first operand of `%s` instruction, got `%s`.", instrToken->lexeme, nextToken->lexeme);

	parser->currentTokenIndex++;
	nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type != TK_REGISTER) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected a register as the second operand of `%s` instruction, got `%s`.", instrToken->lexeme, nextToken->lexeme);
	regNum = normalizeRegister(nextToken->lexeme);
	if (regNum == -1) emitError(ERR_INVALID_REGISTER, &linedata, "Invalid register: `%s`.", nextToken->lexeme);

	Node* xsNode = initASTNode(AST_LEAF, ND_REGISTER, nextToken, instrRoot);
	RegNode* xsData = initRegisterNode(regNum);
	setNodeData(xsNode, xsData, ND_REGISTER);

	parser->currentTokenIndex++;
	nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type != TK_COMMA) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected `,` after second operand of `%s` instruction, got `%s`.", instrToken->lexeme, nextToken->lexeme);

	parser->currentTokenIndex++;
	nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type != TK_REGISTER) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected a register as the third operand of `%s` instruction, got `%s`.", instrToken->lexeme, nextToken->lexeme);
	regNum = normalizeRegister(nextToken->lexeme);
	if (regNum == -1) emitError(ERR_INVALID_REGISTER, &linedata, "Invalid register: `%s`.", nextToken->lexeme);

	Node* xrNode = initASTNode(AST_LEAF, ND_REGISTER, nextToken, instrRoot);
	RegNode* xrData = initRegisterNode(regNum);
	setNodeData(xrNode, xrData, ND_REGISTER);

	instrRoot->nodeData.instruction->data.rType.xd = xdNode;
	instrRoot->nodeData.instruction->data.rType.xs = xsNode;
	instrRoot->nodeData.instruction->data.rType.xr = xrNode;

	parser->currentTokenIndex++;
}

void handleM(Parser* parser, Node* instrRoot) {}

void handleBi(Parser* parser, Node* instrRoot) {
	initScope("handleBi");

	Token* instrToken = parser->tokens[parser->currentTokenIndex];
	linedata_ctx linedata = {
		.linenum = instrToken->linenum,
		.source = ssGetString(instrToken->sstring)
	};

	log("Handling Bi instruction at line %d", instrToken->linenum);

	// enum Instructions instrType = instrRoot->nodeData.instruction->instruction;

	// Instructions are in `label` form, that it, the only operand needs to be a single label (not that not a number but an identifier)

	parser->currentTokenIndex++;
	Token* nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type == TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected a symbol as the operand of `%s` instruction, got nothing.", instrToken->lexeme);
	if (nextToken->type == TK_REGISTER) {
		// User may confuse `ub` and `ubr` (this uses a register), try guiding them
		emitError(ERR_INVALID_SYNTAX, &linedata, "Expected a symbol as the operand of `%s` instruction, got a register: `%s`. Did you mean instruction `ubr`?", 
				instrToken->lexeme, nextToken->lexeme);
	}
	if (nextToken->type != TK_IDENTIFIER) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected a symbol as the operand of `%s` instruction, got `%s`.", instrToken->lexeme, nextToken->lexeme);

	// As mentioned before, the lexer bunches many things as TK_IDENTIFER
	// Need to make sure it is a valid symbol/label
	validateSymbolToken(nextToken, &linedata);

	Node* symbNode = initASTNode(AST_LEAF, ND_SYMB, nextToken, instrRoot);
	uint32_t value = 0;
	int symbTableIndex = -1;
	// Since the label may refer to an already-seen one or to-be-seen, check if it is in the symbol table
	symb_entry_t* symbEntry = getSymbolEntry(parser->symbolTable, nextToken->lexeme);
	if (!symbEntry) {
		// Not found, create an empty entry, also mark a reference
		SYMBFLAGS flags = CREATE_FLAGS(M_NONE, T_NONE, E_EXPR, S_UNDEF, L_LOC, R_REF, D_UNDEF);
		symbEntry = initSymbolEntry(nextToken->lexeme, flags, symbNode, 0, NULL, -1);
		addSymbolEntry(parser->symbolTable, symbEntry);
		symbTableIndex = parser->symbolTable->size - 1;
	} else {
		// Just mark that is has been referenced
		SET_REFERENCED(symbEntry->flags);
		symbTableIndex = symbEntry->symbTableIndex;
	}

	// Add a reference to this location
	addSymbolReference(symbEntry, instrToken->sstring, instrToken->linenum);

	// Set the node data
	SymbNode* symbData = initSymbolNode(symbTableIndex, value);
	setNodeData(symbNode, symbData, ND_SYMB);

	instrRoot->nodeData.instruction->data.biType.offset = symbNode;

	// Make sure nothing is left
	parser->currentTokenIndex++;
	nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type != TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected newline after operand of `%s` instruction, got `%s`.", instrToken->lexeme, nextToken->lexeme);
	parser->currentTokenIndex++;
}

void handleBu(Parser* parser, Node* instrRoot) {
	initScope("handleBu");

	Token* instrToken = parser->tokens[parser->currentTokenIndex];
	linedata_ctx linedata = {
		.linenum = instrToken->linenum,
		.source = ssGetString(instrToken->sstring)
	};

	log("Handling Bu instruction at line %d", instrToken->linenum);

	// Instructions are in the form of `xd`, except for ret, although ret is an alias of `ubr lr`

	enum Instructions instrType = instrRoot->nodeData.instruction->instruction;

	int regNum = 0;

	parser->currentTokenIndex++;
	Token* nextToken = parser->tokens[parser->currentTokenIndex];
	Token* xdToken = nextToken;
	if (instrType != RET) { // aka instrType == UBR
		if (nextToken->type != TK_REGISTER) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected a register as the operand of `%s` instruction, got `%s`.", instrToken->lexeme, nextToken->lexeme);
		regNum = normalizeRegister(nextToken->lexeme);
		if (regNum == -1) emitError(ERR_INVALID_REGISTER, &linedata, "Invalid register: `%s`.", nextToken->lexeme);

		parser->currentTokenIndex++;
		nextToken = parser->tokens[parser->currentTokenIndex];
	} else {
		regNum = 28; // lr
		xdToken = NULL; // Since ret does not have an explicit operand, set xdToken to NULL
		// Let the code generator take care of the implicit register when it detects that it is NULL
	}

	Node* xdNode = initASTNode(AST_LEAF, ND_REGISTER, xdToken, instrRoot);
	RegNode* xdData = initRegisterNode(regNum);
	setNodeData(xdNode, xdData, ND_REGISTER);

	instrRoot->nodeData.instruction->data.buType.xd = xdNode;

	// Regardless if instruction was ubr or ret, nextToken is after the operand or instruction
	if (nextToken->type != TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected newline after operand of `%s` instruction, got `%s`.", instrToken->lexeme, nextToken->lexeme);
	parser->currentTokenIndex++;
}

void handleBc(Parser* parser, Node* instrRoot) {
	initScope("handleBc");

	Token* instrToken = parser->tokens[parser->currentTokenIndex];
	linedata_ctx linedata = {
		.linenum = instrToken->linenum,
		.source = ssGetString(instrToken->sstring)
	};

	log("Handling Bc instruction at line %d", instrToken->linenum);

	// Instructions are in `label` form, that is, it is near the same as Bi except that it has a condition code

	// Check condition code

	char* condStr = instrToken->lexeme + 1; // Skip the 'b'
	int index = indexOf(CONDS, sizeof(CONDS)/sizeof(CONDS[0]), condStr);
	if (index == -1) emitError(ERR_INVALID_INSTRUCTION, &linedata, "Invalid condition code `%s`.", condStr);

	// Coincidentally (or even on purpose ;)), the numeric value of the condition is the same as its index in CONDS
	Node* condASTNode = initASTNode(AST_LEAF, ND_NUMBER, instrToken, instrRoot);
	NumNode* condNode = initNumberNode(NTYPE_INT8, index, 0.0);
	setNodeData(condASTNode, condNode, ND_NUMBER);
	instrRoot->nodeData.instruction->data.bcType.cond = condASTNode;

	// The rest is the same as Bi
	// Do I dare just call `handleBc`???
	// Just kidding no, since `handleBc` does `instrRoot->nodeData.instruction->data.biType.offset = symbNode;` unless some changes are made....
	// But for now, copy and paste and mildly modify

	// COPY AND PASTED FROM `Bi` AND MILDLY MODIFIED

	parser->currentTokenIndex++;
	Token* nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type == TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected a symbol as the operand of `%s` instruction, got nothing.", instrToken->lexeme);
	if (nextToken->type != TK_IDENTIFIER) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected a symbol as the operand of `%s` instruction, got `%s`.", instrToken->lexeme, nextToken->lexeme);

	// As mentioned before, the lexer bunches many things as TK_IDENTIFER
	// Need to make sure it is a valid symbol/label
	validateSymbolToken(nextToken, &linedata);

	Node* symbNode = initASTNode(AST_LEAF, ND_SYMB, nextToken, instrRoot);
	uint32_t value = 0;
	int symbTableIndex = -1;
	// Since the label may refer to an already-seen one or to-be-seen, check if it is in the symbol table
	symb_entry_t* symbEntry = getSymbolEntry(parser->symbolTable, nextToken->lexeme);
	if (!symbEntry) {
		// Not found, create an empty entry, also mark a reference
		SYMBFLAGS flags = CREATE_FLAGS(M_NONE, T_NONE, E_EXPR, S_UNDEF, L_LOC, R_REF, D_UNDEF);
		symbEntry = initSymbolEntry(nextToken->lexeme, flags, symbNode, 0, NULL, -1);
		addSymbolEntry(parser->symbolTable, symbEntry);
		symbTableIndex = parser->symbolTable->size - 1;
	} else {
		// Just mark that is has been referenced
		SET_REFERENCED(symbEntry->flags);
		symbTableIndex = symbEntry->symbTableIndex;
	}

	// Add a reference to this location
	addSymbolReference(symbEntry, instrToken->sstring, instrToken->linenum);

	// Set the node data
	SymbNode* symbData = initSymbolNode(symbTableIndex, value);
	setNodeData(symbNode, symbData, ND_SYMB);

	instrRoot->nodeData.instruction->data.bcType.offset = symbNode;

	// Make sure nothing is left
	parser->currentTokenIndex++;
	nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type != TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected newline after operand of `%s` instruction, got `%s`.", instrToken->lexeme, nextToken->lexeme);
	parser->currentTokenIndex++;
}

void handleS(Parser* parser, Node* instrRoot) {}

void handleF(Parser* parser, Node* instrRoot) {}