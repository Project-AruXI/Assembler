#include <stdlib.h>
#include <ctype.h>

#include "parser.h"
#include "diagnostics.h"
#include "reserved.h"
#include "handlers.h"
#include "expr.h"


Parser* initParser(Token** tokens, int tokenCount, ParserConfig config) {
	Parser* parser = (Parser*) malloc(sizeof(Parser));
	if (!parser) emitError(ERR_MEM, NULL, "Failed to allocate memory for parser.");

	parser->tokens = tokens;
	parser->tokenCount = tokenCount;
	parser->currentTokenIndex = 0;

	parser->asts = (Node**) malloc(sizeof(Node*) * 4);
	if (!parser->asts) emitError(ERR_MEM, NULL, "Failed to allocate memory for parser ASTs.");

	parser->astCount = 0;
	parser->astCapacity = 4;

	parser->ldimmList = NULL;
	parser->ldimmTail = NULL;

	parser->config = config;

	parser->processing = true;

	return parser;
}

void setTables(Parser* parser, SectionTable* sectionTable, SymbolTable* symbolTable, StructTable* structTable, DataTable* dataTable, RelocTable* relocTable) {
	parser->sectionTable = sectionTable;
	parser->symbolTable = symbolTable;
	parser->structTable = structTable;
	parser->dataTable = dataTable;
	parser->relocTable = relocTable;	
}

void deinitParser(Parser* parser) {
	for (int i = 0; i < parser->astCount; i++) {
		freeAST(parser->asts[i]);
	}
	free(parser->asts);

	free(parser);
}

static void addAst(Parser* parser, Node* ast) {
	if (parser->astCount == parser->astCapacity) {
		parser->astCapacity += 2;
		Node** temp = (Node**) realloc(parser->asts, sizeof(Node*) * parser->astCapacity);
		if (!temp) emitError(ERR_MEM, NULL, "Failed to reallocate memory for parser ASTs.");
		parser->asts = temp;
	}
	parser->asts[parser->astCount++] = ast;
}


static void parseLabel(Parser* parser) {
	Token* labelToken = parser->tokens[parser->currentTokenIndex++];

	// Make sure the label is valid
	if (labelToken->lexeme[0] != '_' && !isalpha(labelToken->lexeme[0])) {
		linedata_ctx linedata = {
			.linenum = labelToken->linenum,
			.source = ssGetString(labelToken->sstring)
		};
		emitError(ERR_INVALID_LABEL, &linedata, "Label must start with an alphabetic character or underscore: `%s`", labelToken->lexeme);
	}
	
	// Now, because of how the lexer works, the lexeme should have stopped at a non-alphanumeric character/underscore
	// This shall be the big assumption here
	// So next is just making sure the label does not use a reserved word	
	// Need to compare against REGISTERS, DIRECTIVES, and INSTRUCTIONS array without caring for case
	if (indexOf(REGISTERS, sizeof(REGISTERS)/sizeof(REGISTERS[0]), labelToken->lexeme) != -1 ||
			indexOf(DIRECTIVES, sizeof(DIRECTIVES)/sizeof(DIRECTIVES[0]), labelToken->lexeme) != -1 ||
			indexOf(INSTRUCTIONS, sizeof(INSTRUCTIONS)/sizeof(INSTRUCTIONS[0]), labelToken->lexeme) != -1) {
		linedata_ctx linedata = {
			.linenum = labelToken->linenum,
			.source = ssGetString(labelToken->sstring)
		};
		emitError(ERR_INVALID_LABEL, &linedata, "Label cannot be a reserved word: `%s`", labelToken->lexeme);
	}

	// Ensure that the symbol has not been defined
	// Note that this means an entry can exist but it is only in the case that it has been referenced before
	// If it has been referenced, just updated the defined status

	symb_entry_t* existingEntry = getSymbolEntry(parser->symbolTable, labelToken->lexeme);
	if (existingEntry && GET_DEFINED(existingEntry->flags)) {
		linedata_ctx linedata = {
			.linenum = labelToken->linenum,
			.source = ssGetString(labelToken->sstring)
		};
		emitError(ERR_REDEFINED, &linedata, "Symbol redefinition: `%s`. First defined at `%s`", labelToken->lexeme, ssGetString(existingEntry->source));
	} else if (existingEntry) {
		// Update the existing entry to be defined now
		SET_DEFINED(existingEntry->flags);
		CLR_EXPRESSION(existingEntry->flags);

		uint8_t mainType = 0;
		switch (parser->sectionTable->activeSection) {
			case DATA_SECT_N: mainType = M_OBJ; break;
			case CONST_SECT_N: mainType = M_OBJ; break;
			case BSS_SECT_N: mainType = M_OBJ; break;
			case TEXT_SECT_N: mainType = M_FUNC; break;
			default: mainType = M_NONE; break;
		}

		existingEntry->flags = SET_MAIN_TYPE(existingEntry->flags, mainType);
		// And the section
		existingEntry->flags = SET_SECTION(existingEntry->flags, parser->sectionTable->activeSection);
		existingEntry->linenum = labelToken->linenum;
		existingEntry->source = labelToken->sstring; // Maybe have the entry use its own copy of SString
		existingEntry->value.val = parser->sectionTable->entries[parser->sectionTable->activeSection].lp;
	} else {
		uint8_t mainType = 0;
		switch (parser->sectionTable->activeSection) {
			case DATA_SECT_N: mainType = M_OBJ; break;
			case CONST_SECT_N: mainType = M_OBJ; break;
			case BSS_SECT_N: mainType = M_OBJ; break;
			case TEXT_SECT_N: mainType = M_FUNC; break;
			default: mainType = M_NONE; break;
		}

		SYMBFLAGS flags = CREATE_FLAGS(mainType, T_NONE, E_VAL, parser->sectionTable->activeSection, L_LOC, R_NREF, D_DEF);
		uint32_t addr = parser->sectionTable->entries[parser->sectionTable->activeSection].lp;
		symb_entry_t* symbEntry = initSymbolEntry(labelToken->lexeme, flags, NULL, addr, labelToken->sstring, labelToken->linenum);
		if (!symbEntry) emitError(ERR_MEM, NULL, "Failed to create symbol table entry for label.");

		// Add the symbol entry to the symbol table
		addSymbolEntry(parser->symbolTable, symbEntry);
	}
}

static void parseIdentifier(Parser* parser) {
	initScope("parseIdentifier");

	// The only identifies visible at the "top" level aside from labels are instructions

	Token* idToken = parser->tokens[parser->currentTokenIndex];

	Node* instructionRoot = initASTNode(AST_ROOT, ND_INSTRUCTION, idToken, NULL);

	linedata_ctx linedata = {
		.linenum = idToken->linenum,
		.source = ssGetString(idToken->sstring)
	};

	// Ensure it is an instruction

	int index = indexOf(INSTRUCTIONS, sizeof(INSTRUCTIONS)/sizeof(INSTRUCTIONS[0]), idToken->lexeme);
	// Checking for b{cond} will result in a false negative, so check for that specifically
	// Just treat it as a normal branch, the condition checking will be handled in the handler
	if (index == -1 && tolower(idToken->lexeme[0]) == 'b' && sdslen(idToken->lexeme) == 3) index = B;
	if (index == -1) emitError(ERR_INVALID_INSTRUCTION, &linedata, "Unknown instruction: `%s`", idToken->lexeme);

	idToken->type = TK_INSTRUCTION;

	// Make sure current section is either text, evt, or ivt
	if (parser->sectionTable->activeSection != TEXT_SECT_N && 
			parser->sectionTable->activeSection != EVT_SECT_N &&
			parser->sectionTable->activeSection != IVT_SECT_N) {
		emitError(ERR_INSTR_NOT_IN_TEXT, &linedata, "Instruction `%s` found outside of .text, .evt, or .ivt section.", idToken->lexeme);
	}

	enum Instructions instruction = (enum Instructions) index;
	// log("Parsing instruction: `%s`. Set type to `%s`", idToken->lexeme, INSTRUCTIONS[instruction]);

	InstrNode* instructionData = initInstructionNode(instruction, parser->sectionTable->activeSection);
	setNodeData(instructionRoot, instructionData, ND_INSTRUCTION);

	// Go with same system as the old/legacy assembler
	// Is this the case anymore??????

	if (instruction >= END_TYPE_IDX) emitError(ERR_INTERNAL, &linedata, "Instruction `%s` could not be categorized into a type.", idToken->lexeme);

	// In the case that the instruction is ld immediate/move form, the handler may need to add new ASTs for the decomposition
	// By placing the adding of the AST here, the decomposed ASTs will follow it
	// That allows for the codegen to detect the ld imm/move form and read the following ASTs as part of the instruction
	addAst(parser, instructionRoot);

	if (instruction >= IR_TYPE_IDX && instruction < I_TYPE_IDX) handleIR(parser, instructionRoot);
	else if (instruction >= I_TYPE_IDX && instruction < R_TYPE_IDX) handleI(parser, instructionRoot);
	else if (instruction >= R_TYPE_IDX && instruction < M_TYPE_IDX) handleR(parser, instructionRoot);
	else if (instruction >= M_TYPE_IDX && instruction < Bi_TYPE_IDX) handleM(parser, instructionRoot);
	else if (instruction >= Bi_TYPE_IDX && instruction < Bu_TYPE_IDX) handleBi(parser, instructionRoot);
	else if (instruction >= Bu_TYPE_IDX && instruction < Bc_TYPE_IDX) handleBu(parser, instructionRoot);
	else if (instruction >= Bc_TYPE_IDX && instruction < S_TYPE_IDX) handleBc(parser, instructionRoot);
	else if (instruction >= S_TYPE_IDX && instruction < F_TYPE_IDX) handleS(parser, instructionRoot);
	else if (instruction >= F_TYPE_IDX) handleF(parser, instructionRoot);

	parser->sectionTable->entries[parser->sectionTable->activeSection].lp += 4;
}

static void parseDirective(Parser* parser) {
	initScope("parseDirective");

	Token* directiveToken = parser->tokens[parser->currentTokenIndex];

	Node* directiveRoot = initASTNode(AST_ROOT, ND_DIRECTIVE, directiveToken, NULL);
	if (!directiveRoot) emitError(ERR_MEM, NULL, "Failed to create AST node for directive.");

	linedata_ctx linedata = {
		.linenum = directiveToken->linenum,
		.source = ssGetString(directiveToken->sstring)
	};

	// Since the lexer bunched all directives as TK_DIRECTIVE, the actual directive needs to be determined
	// The token field will also be updated to reflect the actual directive

	int index = indexOf(DIRECTIVES, sizeof(DIRECTIVES)/sizeof(DIRECTIVES[0]), directiveToken->lexeme+1);

	if (index == -1) emitError(ERR_INVALID_DIRECTIVE, &linedata, "Unknown directive: `%s`", directiveToken->lexeme+1);

	// To properly identify the directive
	enum Directives directive = (enum Directives) index;

	// log("Parsing directive: `%s`. Set type to `%s`", directiveToken->lexeme, DIRECTIVES[directive]);

	// The actions depend on the specific directive

	switch (directive) {
		case DATA: handleData(parser); break;
		case CONST: handleConst(parser); break;
		case BSS: handleBss(parser); break;
		case TEXT: handleText(parser); break;
		case EVT: handleEvt(parser); break;
		case IVT: handleIvt(parser); break;

		case SET: handleSet(parser, directiveRoot); break;
		case GLOB: handleGlob(parser, directiveRoot); break;
		case END:
			parser->currentTokenIndex++;
			emitWarning(WARN_UNEXPECTED, &linedata, "The `.end` directive has been encountered. Further lines will be ignored.");
			parser->processing = false;
			break;
		case STRING: handleString(parser, directiveRoot); break;
		case BYTE: handleByte(parser, directiveRoot); break;
		case HWORD: handleHword(parser, directiveRoot); break;
		case WORD: handleWord(parser, directiveRoot); break;
		case FLOAT: handleFloat(parser, directiveRoot); break;
		case ZERO: handleZero(parser, directiveRoot); break;
		case FILL: handleFill(parser, directiveRoot); break;

		case SIZE: handleSize(parser, directiveRoot); break;
		case EXTERN: handleExtern(parser, directiveRoot); break;
		case ALIGN:
			parser->currentTokenIndex++;
			emitWarning(WARN_UNIMPLEMENTED, &linedata, "Directive `%s` not yet implemented!", directiveToken->lexeme);
			break;
		case TYPE: handleType(parser, directiveRoot); break;
		case DEF: handleDef(parser, directiveRoot); break;
		case INCLUDE: handleInclude(parser); break;
		case SIZEOF:
		case TYPEINFO:
			parser->currentTokenIndex++;
			emitWarning(WARN_UNIMPLEMENTED, &linedata, "Directive `%s` not yet implemented!", directiveToken->lexeme);
			break;
		default:
			emitError(ERR_INVALID_DIRECTIVE, &linedata, "Unknown directive: `%s`", directiveToken->lexeme);
			break;
	}

	if (directiveRoot->nodeData.directive) directiveRoot->nodeData.directive->section = parser->sectionTable->activeSection;

	addAst(parser, directiveRoot);
}

static void handleLDImmMove(Parser* parser, Node* ldInstrNode, uint32_t lp) {
	initScope("handleLDImmMove");

	// This is just a defered decomposition from the handler (refer to comment)

	/**
	 * LD move can:
	 * - move a locally-defined absolute value
	 * - move a locally-defined address
	 * - move an extern (but absolute) value
	 * - move an extern (but address) value
	 * 
	 * No relocation is needed for locally-defined absolute, but it is needed for locally-defined address
	 * For externs, since the assembler cannot know the type, it will emit a relocation for both (absolute or address)
	 * Leave it to the linker that, once knowing all symbols, determine whether that relocation is needed or not
	 * 
	 * Hence:
	 * - locally-defined absolute: decomp only
	 * - locally-defined address: reloc + decomp
	 * - extern absolute: reloc + decomp
	 * - extern address: reloc + decomp
	 */


	Node* literalOrImmNode = ldInstrNode->nodeData.instruction->data.mType.imm;
	// Since this could be either =imm or imm, check whether the first node is =
	// In that case, the true imm is its child
	Node* immNode = NULL;
	if (literalOrImmNode->token->type == TK_LITERAL) immNode = literalOrImmNode->nodeData.operator->data.unary.operand;
	else immNode = literalOrImmNode;

	bool evald = evaluateExpression(immNode, parser->symbolTable);
	// When evald is false, it means that an undefined symbol was used
	// Make sure that:
	// The expression is at maximum two operands with either + or - as the operand
	// One operand must be a symbol, the other must be a number
	// The symbol has been declared extern
	// It needs to also apply to local address even thought it is not extern

	linedata_ctx linedata = {
		.linenum = ldInstrNode->token->linenum,
		.source = ssGetString(ldInstrNode->token->sstring)
	};

	Node* externSymbol = getExternSymbol(immNode);
	bool isLocalAddress = false;
	if (externSymbol) {
		int idx = externSymbol->nodeData.symbol->symbTableIndex;
		symb_entry_t* symbEntry = parser->symbolTable->entries[idx];
		if (GET_MAIN_TYPE(symbEntry->flags) != M_ABS && GET_SECTION(symbEntry->flags) != S_UNDEF) {
			isLocalAddress = true;
			log("LD move form instruction immediate is a locally-defined address. Will do relocation as well.");
		}
	}

	if (!evald || isLocalAddress) {
		if (!externSymbol) {
			linedata_ctx linedata = {
				.linenum = ldInstrNode->token->linenum,
				.source = ssGetString(ldInstrNode->token->sstring)
			};
			emitError(ERR_INVALID_EXPRESSION, &linedata, "Failed to get extern symbol for LD immediate form instruction.");
		}

		symb_entry_t* symbEntry = getSymbolEntry(parser->symbolTable, externSymbol->token->lexeme);
		if (!symbEntry) {
			// If no entry at this point (all symbols should have been collected)
			// Something went horribly wrong
			emitError(ERR_INTERNAL, &linedata, "Failed to find symbol table entry for extern symbol in LD immediate form instruction.");
		}
		
		// Make sure the symbol is extern in the case of no result
		if (!evald) {
			uint8_t sect = GET_SECTION(symbEntry->flags);
			if (sect != S_UNDEF) {
				emitError(ERR_INVALID_EXPRESSION, &linedata, "Undefined symbol `%s` used in LD move form instruction is not declared extern.", externSymbol->token->lexeme);
			}
		}


		int32_t addend = 0;
		// Check if there is an operator node
		// In that case, get the number node
		// Also make sure imm is set as 0
		if (immNode->nodeType == ND_OPERATOR) {
			immNode->nodeData.operator->value = 0;

			if (immNode->nodeData.operator->data.binary.left->nodeType == ND_NUMBER) {
				addend = immNode->nodeData.operator->data.binary.left->nodeData.number->value.int32Value;
			} else if (immNode->nodeData.operator->data.binary.right->nodeType == ND_NUMBER) {
				addend = immNode->nodeData.operator->data.binary.right->nodeData.number->value.int32Value;
			} else {
				emitError(ERR_INTERNAL, &linedata, "Failed to find number node for addend in LD move form instruction.");
			}

		} else if (immNode->nodeType == ND_SYMB) {
			immNode->nodeData.symbol->value = 0;
		} else {
			emitError(ERR_INTERNAL, &linedata, "Unexpected node type in LD move form instruction immediate field.");
		}

		RelocEnt* reloc = initRelocEntry(lp, symbEntry->symbTableIndex, RELOC_TYPE_DECOMP, addend);
		addRelocEntry(parser->relocTable, ldInstrNode->nodeData.instruction->section, reloc);
	}

	decomposeLD(ldInstrNode, ldInstrNode->nodeData.instruction->data.mType.xds, immNode);
}

void parse(Parser* parser) {
	initScope("parse");

	int currentTokenIndex = 0;

	while (currentTokenIndex < parser->tokenCount) {
		Token* token = parser->tokens[currentTokenIndex];
		parser->currentTokenIndex = currentTokenIndex;

		switch (token->type) {
			case TK_LABEL:
				parseLabel(parser);
				break;
			case TK_IDENTIFIER:
				// Recall that the lexer bunched many things as TK_IDENTIFIER
				// This is where we determine the actual type
				parseIdentifier(parser);
				break;
			case TK_DIRECTIVE:
				// Similar to TK_IDENTIFIER, the lexer just bunched all of them under TK_DIRECTIVE
				// This is where the specific directive is determined
				parseDirective(parser);
				break;
			case TK_MACRO:
			case TK_OUT:
			case TK_REGISTER:
			case TK_IMM:
			case TK_COMMA:
			case TK_LPAREN:
			case TK_RPAREN:
			case TK_LSQBRACKET:
			case TK_RSQBRACKET:
			case TK_LBRACKET:
			case TK_RBRACKET:
			case TK_COLON:
			case TK_COLON_COLON:
			case TK_STRING:
			case TK_DOT:
			case TK_PLUS:
			case TK_MINUS:
			case TK_ASTERISK:
			case TK_DIVIDE:
			case TK_LITERAL:
			case TK_BITWISE_AND:
			case TK_BITWISE_OR:
			case TK_BITWISE_XOR:
			case TK_BITWISE_NOT:
			case TK_LP:
			case TK_MACRO_ARG:
			case TK_INTEGER:
			case TK_FLOAT:
			case TK_CHAR:
			case TK_IF:
			case TK_MAIN_TYPE:
			case TK_SUB_TYPE:
			// All of these token types typically follow a directive, an identifier, or a label
			// So these are to be parsed in their respective contexts
			// Encountering them at the top level is an issue
			linedata_ctx linedata = {
				.linenum = token->linenum,
				.source = ssGetString(token->sstring)
			};
			emitError(ERR_INVALID_SYNTAX, &linedata, "Unexpected token: `%s`", token->lexeme);
			break;
			case TK_NEWLINE:
			// Newlines can either appear at the end of a statement or on their own line
			// The end-of-statement newlines are handled in the respective handlers
			// So newlines that appear here are just standalone newlines
			// Ignore, but make sure to advance
			parser->currentTokenIndex++;
			break;
			default: emitError(ERR_INTERNAL, NULL, "Parser encountered unhandled token type: %s", token->lexeme);
		}
		currentTokenIndex = parser->currentTokenIndex;

		if (!parser->processing) {
			emitWarning(WARN_UNEXPECTED, NULL, "Parser has encountered a .end directive. Further lines will be ignored.");
			break;
		}
	}

	// rlog("Parsing complete. Will now fix any LD imm instructions.");
	// All symbols have been gathered
	// Try to fix the LD imm/move instructions
	struct LDIMM* current = parser->ldimmList;
	while (current) {
		handleLDImmMove(parser, current->ldInstr, current->lp);

		current = current->next;
	}

	// Set each section's size to be the LP
	for (int i = 0; i < 6; i++) {
		parser->sectionTable->entries[i].size = parser->sectionTable->entries[i].lp;
	}
}

void showParserConfig(Parser* parser) {
	log("Parser Configuration");

}

void addLD(Parser* parser, Node* ldInstrNode) {
	// Adds a ld immediate/move possible decomposition to the parser's list
	// Use ldimmTail to make this O(1)
	struct LDIMM* newLDIMM = (struct LDIMM*) malloc(sizeof(struct LDIMM));
	if (!newLDIMM) emitError(ERR_MEM, NULL, "Failed to allocate memory for LDIMM node.");
	newLDIMM->ldInstr = ldInstrNode;
	newLDIMM->next = NULL;
	newLDIMM->lp = parser->sectionTable->entries[parser->sectionTable->activeSection].lp;

	if (!parser->ldimmList) {
		parser->ldimmList = newLDIMM;
		parser->ldimmTail = newLDIMM;
	} else {
		parser->ldimmTail->next = newLDIMM;
		parser->ldimmTail = newLDIMM;
	}
}