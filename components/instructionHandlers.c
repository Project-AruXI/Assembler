#include <ctype.h>

#include "handlers.h"
#include "ast.h"
#include "diagnostics.h"
#include "SymbolTable.h"
#include "expr.h"


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
	
	if (nextToken->type == TK_IMM || nextToken->type == TK_INTEGER || nextToken->type == TK_IDENTIFIER || 
			nextToken->type == TK_LPAREN || nextToken->type == TK_PLUS || nextToken->type == TK_MINUS || nextToken->type == TK_RPAREN || 
			nextToken->type == TK_LP) {
		Node* immExprRoot = parseExpression(parser);

		// Immediate, so I-type with two operands
		immExprRoot->parent = instrRoot;
		// Node* immNode = initASTNode(AST_LEAF, ND_NUMBER, nextToken, instrRoot);
		// NumNode* immData = initNumberNode(NTYPE_UINT14, atoi(nextToken->lexeme + 1), 0.0);
		// setNodeData(immNode, immData, ND_NUMBER);

		// Set the instruction data
		if (instrType == CMP) {
			// Temporarily directly set, need to use a function later
			instrRoot->nodeData.instruction->data.iType.xs = xsNode;
			instrRoot->nodeData.instruction->data.iType.imm = immExprRoot;
		} else {
			instrRoot->nodeData.instruction->data.iType.xd = xdNode;
			instrRoot->nodeData.instruction->data.iType.imm = immExprRoot;
		}

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
				Node* immExprRoot = parseExpression(parser);

				// Definitive I-type with three operands
				immExprRoot->parent = instrRoot;
				// Node* immNode = initASTNode(AST_LEAF, ND_NUMBER, nextToken, instrRoot);
				// NumNode* immData = initNumberNode(NTYPE_UINT14, atoi(nextToken->lexeme + 1), 0.0);
				// setNodeData(immNode, immData, ND_NUMBER);

				instrRoot->nodeData.instruction->data.iType.xd = xdNode;
				instrRoot->nodeData.instruction->data.iType.xs = xsNode;
				instrRoot->nodeData.instruction->data.iType.imm = immExprRoot;

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

static Node* parseMemberAccess(Parser* parser) {
	initScope("parseMemberAccess");

	// This function assumes that the current token is a TK_IDENTIFIER
	// It will parse the member access/dereference chain and return the root of the expression tree
	// The tree will be in the form of:
	// symbol -|
	//         |- . -|
	//               |- member (TK_IDENTIFIER)
	//               |- . -|
	//                     |- member (TK_IDENTIFIER)
	//                     |- -> -|
	//                           |- member (TK_IDENTIFIER)
	//                           |- ... and so on

	Token* symbolToken = parser->tokens[parser->currentTokenIndex];
	linedata_ctx linedata = {
		.linenum = symbolToken->linenum,
		.source = ssGetString(symbolToken->sstring)
	};

	// Need to get the symbol itself and the type
	symb_entry_t* symbol = getSymbolEntry(parser->symbolTable, symbolToken->lexeme);
	int subType = GET_SUB_TYPE(symbol->flags);

	if (subType == T_PTR && !FEATURE_ENABLED(parser->config, FEATURE_PTR_DEREF)) {
		emitError(ERR_NOT_ALLOWED, &linedata, "Pointer dereference is not allowed. Enable the feature to use it.");
	}
	if ((subType == T_STRUCT || subType == T_ARR) && !FEATURE_ENABLED(parser->config, FEATURE_FIELD_ACCESS)) {
		emitError(ERR_NOT_ALLOWED, &linedata, "Field access is not allowed. Enable the feature to use it.");
	}

	
}

void handleM(Parser* parser, Node* instrRoot) {
	initScope("handleM");

	Token* instrToken = parser->tokens[parser->currentTokenIndex];
	linedata_ctx linedata = {
		.linenum = instrToken->linenum,
		.source = ssGetString(instrToken->sstring)
	};

	log("Handling M instruction at line %d", instrToken->linenum);

	// M-type instructions are the most tricky as it can vary
	// In general, they have the following forms:
	// - mem-op reg, [reg]
	// - mem-op reg, [reg, imm]
	// - mem-op reg, [reg], reg
	// However, `ld` has two more forms:
	// - ld reg, =imm  <- Basically just a `mv` but on a large number since `mv` only supports 14-bit immediates
	// - ld reg, imm <- Load from the address
	// Expressions can be used where `imm` appears
	// Additionally, if the advanced typing system is enabled, doing member access/dereference is allowed but only
	//   for `ld reg, imm`, for example `ld x0, mySymbol.member`. `mySymbol.member` (if valid) will result in a number that is to act as an addr
	// Also, for `ld reg, imm`, if the immediate/address is close enough to the LP, it does an IR-relative load

	enum Instructions instrType = instrRoot->nodeData.instruction->instruction;

	/**
	 * Trees:
	 * op reg0, [reg1]
	 * op -|
	 * 	   |- reg0 (xds)
	 * 	   |- reg1 (xb)
	 *     |- NULL (xi)
	 * 	   |- NULL (imm)
	 * 
	 * op reg0, [reg1, imm]
	 * op -|
	 *     |- reg0 (xds)
	 * 	   |- reg1 (xb)
	 * 	   |- NULL (xi)
	 * 	   |- imm (imm) -|
	 * 						       |- ... (expression tree)
	 * 
	 * op reg0, [reg1], reg2
	 * op -|
	 * 	   |- reg0 (xds)
	 * 	   |- reg1 (xb)
	 * 	   |- reg2 (xi)
	 * 	   |- NULL (imm)
	 * 
	 * ld reg, imm
	 * ld -|
	 *     |- reg (xds)
	 * 	   |- NULL (xb)
	 * 	   |- NULL (xi)
	 * 	   |- imm (imm) -|
	 * 						       |- ... (expression tree)
	 * 
	 * ld reg, =imm
	 * ld -|
	 *     |- reg (xds)
	 * 	   |- NULL (xb)
	 * 	   |- NULL (xi)
	 * 	   |- = (imm) -|
	 * 						     |- imm -|
	 * 						             |- ... (expression tree)
	 */

	parser->currentTokenIndex++;
	Token* nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type != TK_REGISTER) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected a register, got `%s`.", nextToken->lexeme);
	int regNum = normalizeRegister(nextToken->lexeme);
	if (regNum == -1) emitError(ERR_INVALID_REGISTER, &linedata, "Invalid register: `%s`.", nextToken->lexeme);

	Node* xdsNode = initASTNode(AST_LEAF, ND_REGISTER, nextToken, instrRoot);
	RegNode* xdsData = initRegisterNode(regNum);
	setNodeData(xdsNode, xdsData, ND_REGISTER);
	instrRoot->nodeData.instruction->data.mType.xds = xdsNode;

	parser->currentTokenIndex++;
	nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type != TK_COMMA) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected `,`, got `%s`.", nextToken->lexeme);

	// Now this is where some divergence happens
	// The acceptable next tokens are:
	// - `[` TK_LBRACKET
	// - TK_IMM (#...), TK_SYMBOL, TK_INTEGER, TK_LPAREN, TK_PLUS, TK_MINUS, TK_LP (for ld reg, imm)
	// - `=` TK_LD_IMM (for ld reg, =imm)

	parser->currentTokenIndex++;
	nextToken = parser->tokens[parser->currentTokenIndex];

	trace("Next token after first operand and comma: `%s`; type: %d", nextToken->lexeme, nextToken->type);

	if (nextToken->type == TK_LSQBRACKET) {
		// This means it is one of the first three forms (mem-op reg, [reg]; mem-op reg, [reg, imm]; mem-op reg, [reg], reg)
		// Going to get base register
		parser->currentTokenIndex++;
		nextToken = parser->tokens[parser->currentTokenIndex];
		if (nextToken->type != TK_REGISTER) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected a register, got `%s`.", nextToken->lexeme);
		regNum = normalizeRegister(nextToken->lexeme);
		if (regNum == -1) emitError(ERR_INVALID_REGISTER, &linedata, "Invalid register: `%s`.", nextToken->lexeme);

		Node* xbNode = initASTNode(AST_LEAF, ND_REGISTER, nextToken, instrRoot);
		RegNode* xbData = initRegisterNode(regNum);
		setNodeData(xbNode, xbData, ND_REGISTER);
		instrRoot->nodeData.instruction->data.mType.xb = xbNode;

		parser->currentTokenIndex++;
		nextToken = parser->tokens[parser->currentTokenIndex];
		if (nextToken->type == TK_COMMA) {
			// mem-op reg, [reg, imm]
			parser->currentTokenIndex++;
			nextToken = parser->tokens[parser->currentTokenIndex];

			Node* immExprRoot = parseExpression(parser);

			nextToken = parser->tokens[parser->currentTokenIndex];
			if (nextToken->type != TK_RSQBRACKET) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected `]`, got `%s`.", nextToken->lexeme);

			instrRoot->nodeData.instruction->data.mType.xi = NULL;
			instrRoot->nodeData.instruction->data.mType.imm = immExprRoot;

			parser->currentTokenIndex++;
			nextToken = parser->tokens[parser->currentTokenIndex];
			if (nextToken->type != TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected newline after `]`, got `%s`.", nextToken->lexeme);
			parser->currentTokenIndex++; // Consume the newline
			return;
		} else if (nextToken->type == TK_RSQBRACKET) {
			// mem-op reg, [reg] or mem-op reg, [reg], reg
			parser->currentTokenIndex++;
			nextToken = parser->tokens[parser->currentTokenIndex];
			if (nextToken->type == TK_COMMA) {
				// mem-op reg, [reg], reg
				parser->currentTokenIndex++;
				nextToken = parser->tokens[parser->currentTokenIndex];
				if (nextToken->type != TK_REGISTER) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected a register as the index register, got `%s`.", nextToken->lexeme);
				regNum = normalizeRegister(nextToken->lexeme);
				if (regNum == -1) emitError(ERR_INVALID_REGISTER, &linedata, "Invalid register: `%s`.", nextToken->lexeme);

				Node* xiNode = initASTNode(AST_LEAF, ND_REGISTER, nextToken, instrRoot);
				RegNode* xiData = initRegisterNode(regNum);
				setNodeData(xiNode, xiData, ND_REGISTER);
				instrRoot->nodeData.instruction->data.mType.xi = xiNode;
				instrRoot->nodeData.instruction->data.mType.imm = NULL;

				parser->currentTokenIndex++;
				nextToken = parser->tokens[parser->currentTokenIndex];
				if (nextToken->type != TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected newline after index register, got `%s`.", nextToken->lexeme);
				parser->currentTokenIndex++; // Consume the newline
				return;
			} else if (nextToken->type == TK_NEWLINE) {
				// mem-op reg, [reg]
				instrRoot->nodeData.instruction->data.mType.xi = NULL;
				instrRoot->nodeData.instruction->data.mType.imm = NULL;
				parser->currentTokenIndex++; // Consume the newline
				return;
			}
			emitError(ERR_INVALID_SYNTAX, &linedata, "Expected `,` or newline after `]`, got `%s`.", nextToken->lexeme);
		}
		emitError(ERR_INVALID_SYNTAX, &linedata, "Expected `,` or `]`, got `%s`.", nextToken->lexeme);
	} else if (nextToken->type == TK_IMM || nextToken->type == TK_INTEGER || nextToken->type == TK_IDENTIFIER || 
			nextToken->type == TK_LPAREN || nextToken->type == TK_PLUS || nextToken->type == TK_MINUS || nextToken->type == TK_LP) {
		// Need to make sure it is `ld`
		if (instrType != LD) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected `[`, got `%s`. Only `ld` instruction supports loading from an address.", nextToken->lexeme);

		// Remember this is where the advanced typing system can go
		// It start with the symbol, then the access/dereference chain
		// However, parseExpression cannot handle that, nor it knows what to do
		// Handle it here
		Node* immExprRoot = NULL;

		// Need to make sure that the member access follows, if not, it is just a normal symbol (ie by itself or in an expression)
		Token* peekedToken = parser->tokens[parser->currentTokenIndex + 1];
		if (nextToken->type == TK_IDENTIFIER && peekedToken->type == TK_DOT) {
			// Leave the accessing to a function in order to keep things clear
			// Also make sure the advanced typing system is enabled
			// However, there is pointer dereference and field access
			// Since this does not know the exact system, just check if either is enabled
			if (!FEATURE_ENABLED(parser->config, FEATURE_PTR_DEREF) && !FEATURE_ENABLED(parser->config, FEATURE_FIELD_ACCESS)) {
				emitError(ERR_INVALID_SYNTAX, &linedata, "Member access/dereference is not enabled.");
			}

			immExprRoot = parseMemberAccess(parser);
		} else immExprRoot = parseExpression(parser);

		// Since `imm` is most likely 32 bits, it doesn't fit in the instruction
		// This means to either to IR-relative or split
		// The expression will not be evaluated until later so until then it will be known
		// For know, leave as is, and when it is evaluated, figure out if it can be IR-relative or needs to be split

		instrRoot->nodeData.instruction->data.mType.xb = NULL;
		instrRoot->nodeData.instruction->data.mType.xi = NULL;
		instrRoot->nodeData.instruction->data.mType.imm = immExprRoot;

		immExprRoot->parent = instrRoot;

		nextToken = parser->tokens[parser->currentTokenIndex];
		if (nextToken->type != TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected newline after immediate expression, got `%s`.", nextToken->lexeme);
		parser->currentTokenIndex++; // Consume the newline

		return;
	} else if (nextToken->type == TK_LD_IMM) {
		// ld reg, =imm

		Node* literalNode = initASTNode(AST_INTERNAL, ND_OPERATOR, nextToken, instrRoot);
		OpNode* literalData = initOperatorNode();
		setNodeData(literalNode, literalData, ND_OPERATOR);
		instrRoot->nodeData.instruction->data.mType.imm = literalNode;
		
		parser->currentTokenIndex++;
		nextToken = parser->tokens[parser->currentTokenIndex];
		// Need to check first????
		Node* immExprRoot = parseExpression(parser);
		immExprRoot->parent = literalNode;

		instrRoot->nodeData.instruction->data.mType.xb = NULL;
		instrRoot->nodeData.instruction->data.mType.xi = NULL;
		instrRoot->nodeData.instruction->data.mType.imm = literalNode;

		nextToken = parser->tokens[parser->currentTokenIndex];
		if (nextToken->type != TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected newline after immediate expression, got `%s`.", nextToken->lexeme);
		parser->currentTokenIndex++; // Consume the newline

		return;
	}
	emitError(ERR_INVALID_SYNTAX, &linedata, "Expected `[`, `=`, or an immediate expression as the second operand, got `%s`.", nextToken->lexeme);
}

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

void handleS(Parser* parser, Node* instrRoot) {
	initScope("handleS");

	Token* instrToken = parser->tokens[parser->currentTokenIndex];
	linedata_ctx linedata = {
		.linenum = instrToken->linenum,
		.source = ssGetString(instrToken->sstring)
	};

	log("Handling S instruction at line %d", instrToken->linenum);

	// Instructions are in the form of `xd|xs` for some instructions
	// ldir, ldcstr, resr use xd
	// mvcstr uses xs
	// The rest don't use operands

	enum Instructions instrType = instrRoot->nodeData.instruction->instruction;

	parser->currentTokenIndex++;
	Token* nextToken = parser->tokens[parser->currentTokenIndex];

	if (instrType < LDIR) {
		// Operand-less instructions
		if (nextToken->type != TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected newline after instruction, got `%s`.", nextToken->lexeme);
		parser->currentTokenIndex++;
		return;
	}

	if (nextToken->type != TK_REGISTER) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected a register as the operand, got `%s`.", nextToken->lexeme);
	int regNum = normalizeRegister(nextToken->lexeme);
	if (regNum == -1) emitError(ERR_INVALID_REGISTER, &linedata, "Invalid register: `%s`.", nextToken->lexeme);

	Node* xs_xdNode = initASTNode(AST_LEAF, ND_REGISTER, nextToken, instrRoot);
	RegNode* xs_xdData = initRegisterNode(regNum);
	setNodeData(xs_xdNode, xs_xdData, ND_REGISTER);

	if (instrType == MVCSTR) instrRoot->nodeData.instruction->data.sType.xs = xs_xdNode;
	else instrRoot->nodeData.instruction->data.sType.xd = xs_xdNode;

	parser->currentTokenIndex++;
	nextToken = parser->tokens[parser->currentTokenIndex];
	if (nextToken->type != TK_NEWLINE) emitError(ERR_INVALID_SYNTAX, &linedata, "Expected newline after operand, got `%s`.", nextToken->lexeme);

	parser->currentTokenIndex++;
}

void handleF(Parser* parser, Node* instrRoot) {
	initScope("handleF");

	Token* instrToken = parser->tokens[parser->currentTokenIndex];
	linedata_ctx linedata = {
		.linenum = instrToken->linenum,
		.source = ssGetString(instrToken->sstring)
	};

	emitWarning(WARN_UNIMPLEMENTED, &linedata, "F-type instruction `%s` not yet implemented.", instrToken->lexeme);
	// log("Handling F instruction at line %d", instrToken->linenum);
}