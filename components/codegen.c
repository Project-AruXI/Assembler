#include <string.h>

#include "codegen.h"
#include "expr.h"
#include "diagnostics.h"


CodeGen* initCodeGenerator(SectionTable* sectionTable, SymbolTable* symbolTable, RelocTable* relocTable) {
	CodeGen* codegen = (CodeGen*) malloc(sizeof(CodeGen));
	if (!codegen) emitError(ERR_MEM, NULL, "Failed to allocate memory for code generator.");

	codegen->text.instructions = (uint32_t*) malloc(sizeof(uint32_t) * 16);
	if (!codegen->text.instructions) emitError(ERR_MEM, NULL, "Failed to allocate memory for text section instructions.");
	codegen->text.instructionCount = 0;
	codegen->text.instructionCapacity = 16;

	codegen->data.data = (uint8_t*) malloc(sizeof(uint8_t) * 16);
	if (!codegen->data.data) emitError(ERR_MEM, NULL, "Failed to allocate memory for data section.");
	codegen->data.dataCount = 0;
	codegen->data.dataCapacity = 16;

	codegen->consts.data = (uint8_t*) malloc(sizeof(uint8_t) * 16);
	if (!codegen->consts.data) emitError(ERR_MEM, NULL, "Failed to allocate memory for const section.");
	codegen->consts.dataCount = 0;
	codegen->consts.dataCapacity = 16;

	codegen->evt.data = (uint8_t*) malloc(sizeof(uint8_t) * 16);
	if (!codegen->evt.data) emitError(ERR_MEM, NULL, "Failed to allocate memory for evt section.");
	codegen->evt.dataCount = 0;
	codegen->evt.dataCapacity = 16;

	codegen->sectionTable = sectionTable;
	codegen->symbolTable = symbolTable;
	codegen->relocTable = relocTable;

	return codegen;
}

void deinitCodeGenerator(CodeGen* codegen) {	
	free(codegen->text.instructions);
	free(codegen->data.data);
	free(codegen->consts.data);
	free(codegen->evt.data);
	free(codegen);
}


static uint32_t getImmediateEncoding(Node* immNode, NumType expectedType, SymbolTable* symbTable, RelData* reldata) {
	initScope("getImmediateEncoding");

	log("Getting immediate encoding for %s", (immNode->token ? immNode->token->lexeme : "unknown"));
	printAST(immNode);
	bool evald = evaluateExpression(immNode, symbTable);
	linedata_ctx linedata = {
		.linenum = immNode->token ? immNode->token->linenum : -1,
		.source = immNode->token ? ssGetString(immNode->token->sstring) : NULL
	};
	// evald is false when the immediate expression uses an extern symbol
	// In that case, the immediate is set to 0 and a relocation entry is added
	if (!evald) {
		// If my guess is correct, the same logic as in handleLDMove/Imm applies
		log("Immediate expression could not be fully evaluated, likely due to extern symbol(s). Adding relocation entry.");

		Node* externSymbol = getExternSymbol(immNode);
		if (!externSymbol) {
			linedata_ctx linedata = {
				.linenum = immNode->token->linenum,
				.source = ssGetString(immNode->token->sstring)
			};
			emitError(ERR_INVALID_EXPRESSION, &linedata, "Failed to get extern symbol for immediate.");
		}

		// Make sure the symbol is extern
		symb_entry_t* symbEntry = getSymbolEntry(symbTable, externSymbol->token->lexeme);
		if (!symbEntry) {
			// If no entry at this point (all symbols should have been collected)
			// Something went horribly wrong
			emitError(ERR_INTERNAL, &linedata, "Failed to find symbol table entry for extern symbol in immediate.");
		}

		uint8_t sect = GET_SECTION(symbEntry->flags);
		if (sect != S_UNDEF) {
			emitError(ERR_INVALID_EXPRESSION, &linedata, "Undefined symbol `%s` used in immediate is not declared extern.", externSymbol->token->lexeme);
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
				emitError(ERR_INTERNAL, &linedata, "Failed to find number node for addend in immediate.");
			}
		} else if (immNode->nodeType == ND_SYMB) {
			immNode->nodeData.symbol->value = 0;
		} else {
			emitError(ERR_INTERNAL, &linedata, "Unexpected node type in immediate.");
		}

		reldata->addend = addend;
		RelocEnt* reloc = initRelocEntry(reldata->lp, symbEntry->symbTableIndex, reldata->type, addend);

		// Some relocation types affect certain sections
		// So section can be inferred from those relocations
		// At least for now????
		sect_table_n section = 0;
		switch (reldata->type) {
			case RELOC_TYPE_ABS: case RELOC_TYPE_DECOMP: case RELOC_TYPE_MEM: case RELOC_TYPE_IR24: case RELOC_TYPE_IR19:
				section = TEXT_SECT_N;
				break;
			case RELOC_TYPE_BYTE: case RELOC_TYPE_HWORD: case RELOC_TYPE_WORD:
				section = DATA_SECT_N;
				break;
			default: emitError(ERR_INTERNAL, &linedata, "Unhandled relocation type for immediate.");
		}

		addRelocEntry(reldata->relocTable, section, reloc);

		// When the relocation type is for branching, the immediate is always set to 0
		// This causes it to use 0 on doing the offset math, which should not happen
		// It is to remain as 0
		// Easier way is to set the relTable in reldata as null to indicate
		reldata->relocTable = NULL;

		return 0x0; // externs result in 0
	}

	// immNode can either be an operator, number, or symbol node
	// operator and number nodes have their value directly
	// symbol nodes need to be looked up in the symbol table

	uint32_t value = 0;
	switch (immNode->nodeType) {
		case ND_NUMBER: {
			NumNode* numData = immNode->nodeData.number;
			if (!numData) emitError(ERR_INTERNAL, NULL, "Number node data is NULL.");

			// Check type
			if (numData->type > expectedType) {
				emitError(ERR_INVALID_TYPE, &linedata, "Immediate number type does not match expected type. Expected %d, got %d.", expectedType, numData->type);
			}
			value = (uint32_t) numData->value.int32Value; // Use int32 for now
			break;
		}
		case ND_SYMB: {
			SymbNode* symbData = (SymbNode*) immNode->nodeData.symbol;
			if (!symbData) emitError(ERR_INTERNAL, NULL, "Symbol node data is NULL.");
			int idx = symbData->symbTableIndex;
			if (idx < 0 || idx >= (int)symbTable->size) emitError(ERR_INTERNAL, NULL, "Symbol index %d out of bounds in symbol table.", idx);
			symb_entry_t* entry = symbTable->entries[idx];
			if (!entry) emitError(ERR_INTERNAL, NULL, "Symbol table entry at index %d is NULL.", idx);
			value = entry->value.val;
			break;
		}
		case ND_OPERATOR: {
			// Operator node has value in `value` and `valueType`
			OpNode* opData = (OpNode*) immNode->nodeData.operator;
			if (!opData) emitError(ERR_INTERNAL, NULL, "Operator node data is NULL.");
			// Check type
			if (opData->valueType > expectedType) {
				emitError(ERR_INVALID_TYPE, &linedata, "Immediate operator type does not match expected type.");
			}
			value = opData->value;
			break;
		}
		default:
			emitError(ERR_INTERNAL, &linedata, "Immediate node is of invalid type.");
	}

	log("Immediate encoding value: 0x%X", value);


	return value;
}


static uint32_t encodeI(InstrNode* data, uint32_t lp, SymbolTable* symbTable, RelocTable* relocTable) {
	initScope("encodeI");

	uint32_t encoding = 0x00000000;
#ifdef _WIN64
	uint32_t opcode = 0b0000000;
#else
	uint8_t opcode = 0b0000000;
#endif

	switch (data->instruction) {
		case ADD: case NOP: opcode = 0b10000000; break;
		case ADDS: opcode = 0b10001000; break;
		case SUB: case MVN: opcode = 0b10010000; break;
		case SUBS: case CMP: opcode = 0b10011000; break;
		case OR: opcode = 0b01000000; break;
		case AND: opcode = 0b01000010; break;
		case XOR: opcode = 0b01000100; break;
		case NOT: opcode = 0b01000110; break;
		case LSL: opcode = 0b01001000; break;
		case LSR: opcode = 0b01001010; break;
		case ASR: opcode = 0b01001100; break;
		case MV: opcode = 0b10000100; break;
		default: emitError(ERR_INTERNAL, NULL, "Could not encode instruction `%s`", INSTRUCTIONS[data->instruction]);
	}

	// uint8_t rd = (uint8_t) data->data.iType.xd->nodeData.reg->regNumber;
	uint8_t rd = data->data.iType.xd ? (uint8_t) data->data.iType.xd->nodeData.reg->regNumber : 30;
	uint8_t rs = data->data.iType.xs ? (uint8_t) data->data.iType.xs->nodeData.reg->regNumber : 30;
	
	Node* immNode = data->data.iType.imm;

	// The only time immNode is allowed to be null is when the instruction has no immediate
	// The only I-type instruction to have no immediate is NOP (alias of `add xz, xz, #0`)
	// In such case, just set it to 0

	uint16_t imm14 = 0x0000;

	RelData reldata = {
		.lp = lp,
		.addend = 0,
		.type = RELOC_TYPE_ABS,
		.relocTable = relocTable
	};
	if (immNode) imm14 = (uint16_t) getImmediateEncoding(immNode, NTYPE_UINT14, symbTable, &reldata);
	else {
		// For security, check that the instruction is in fact NOP
		if (data->instruction != NOP) emitError(ERR_INTERNAL, NULL, "Immediate node is NULL for non-NOP instruction.");
	}

	encoding = (opcode << 24) | (imm14 << 10) | (rs << 5) | (rd << 0);

	detail("Encoded I-type instruction `%s`: 0x%08X", INSTRUCTIONS[data->instruction], encoding);
	trace("Opcode: 0b%07b; imm14: 0x%04X; rs: 0x%02X; rd: 0x%02X", opcode, imm14, rs, rd);

	return encoding;
}

static uint32_t encodeR(InstrNode* data) {
	// initScope("encodeR");

	uint32_t encoding = 0x00000000;
#ifdef _WIN64
	uint32_t opcode = 0b0000000;
#else
	uint8_t opcode = 0b0000000;
#endif

	// Note that the only difference in opcode between I and R is the last bit
	// Only exception are truly R types
	// Refer to ISA documentation for more details
	switch (data->instruction) {
		case ADD: opcode = 0b10000001; break;
		case ADDS: opcode = 0b10001001; break;
		case SUB: case MVN: opcode = 0b10010001; break;
		case SUBS: case CMP: opcode = 0b10011001; break;
		case OR: case MV: opcode = 0b01000001; break;
		case AND: opcode = 0b01000011; break;
		case XOR: opcode = 0b01000101; break;
		case NOT: opcode = 0b01000111; break;
		case LSL: opcode = 0b01001001; break;
		case LSR: opcode = 0b01001011; break;
		case ASR: opcode = 0b01001101; break;
		case MUL: opcode = 0b11000001; break;
		case SMUL: opcode = 0b11001001; break;
		case DIV: opcode = 0b11010001; break;
		case SDIV: opcode = 0b11011001; break;
		default: emitError(ERR_INTERNAL, NULL, "Could not encode instruction `%s`", INSTRUCTIONS[data->instruction]);
	}

	uint8_t rd = data->data.rType.xd ? (uint8_t) data->data.rType.xd->nodeData.reg->regNumber : 30;
	uint8_t rs = data->data.rType.xs ? (uint8_t) data->data.rType.xs->nodeData.reg->regNumber : 30;
	uint8_t rr = data->data.rType.xr ? (uint8_t) data->data.rType.xr->nodeData.reg->regNumber : 30;

	encoding = (opcode << 24) | (rs << 10) | (rr << 5) | (rd << 0);

	detail("Encoded R-type instruction `%s`: 0x%08X", INSTRUCTIONS[data->instruction], encoding);
	trace("Opcode: 0b%07b; rs: 0x%02X; rr: 0x%02X; rd: 0x%02X", opcode, rs, rr, rd);

	return encoding;
}

static uint32_t encodeM(InstrNode* data, uint32_t lp, SymbolTable* symbTable, RelocTable* relocTable) {
	initScope("encodeM");

	uint32_t encoding = 0x00000000;
#ifdef _WIN64
	uint32_t opcode = 0b0000000;
#else
	uint8_t opcode = 0b0000000;
#endif

	switch (data->instruction) {
		case LD: opcode = 0b00010100; break;
		case LDB: opcode = 0b00110100; break;
		case LDBS: opcode = 0b01010100; break;
		case LDBZ: opcode = 0b01110100; break;
		case LDH: opcode = 0b10010100; break;
		case LDHS: opcode = 0b10110100; break;
		case LDHZ: opcode = 0b11010100; break;
		case STR: opcode = 0b00011100; break;
		case STRB: opcode = 0b00111100; break;
		case STRH: opcode = 0b01011100; break;
		default: emitError(ERR_INTERNAL, NULL, "Could not encode instruction `%s`", INSTRUCTIONS[data->instruction]);
	}

	uint8_t rd = data->data.mType.xds->nodeData.reg->regNumber;
	// rs is the base register
	// It does not exist when the instruction is LD immediate form but this should already be taken care of
	if (!data->data.mType.xb) emitError(ERR_INTERNAL, NULL, "Base register is NULL. This indicates a LD imm/move which should not be encoded as is.");

	uint8_t rs = data->data.mType.xb->nodeData.reg->regNumber;

	// rr is the optional index register, defaults to 0b11110 (XZ)
	uint8_t rr = data->data.mType.xi ? data->data.mType.xi->nodeData.reg->regNumber : 0b11110;

	Node* immNode = data->data.mType.imm;

	// imm is optional
	int16_t imm9 = 0x0000;

	RelData reldata = {
		.lp = lp,
		.addend = 0,
		.type = RELOC_TYPE_MEM,
		.relocTable = relocTable
	};
	if (immNode) imm9 = (int16_t) getImmediateEncoding(immNode, NTYPE_INT9, symbTable, &reldata);

	encoding = (opcode << 24) | ((imm9 & 0x1FF) << 15) | (rs << 10) | (rr << 5) | (rd << 0);

	detail("Encoded M-type instruction `%s`: 0x%08X", INSTRUCTIONS[data->instruction], encoding);
	trace("Opcode: 0b%07b; imm9: 0x%03X; rs: 0x%02X; rr: 0x%02X; rd: 0x%02X", opcode, (imm9 & 0x1FF), rs, rr, rd);

	return encoding;
}

static uint32_t encodeBu(InstrNode* data) {
	initScope("encodeBu");

	uint32_t encoding = 0x00000000;
#ifdef _WIN64
	uint32_t opcode = 0b0000000;
#else
	uint8_t opcode = 0b0000000;
#endif

	switch (data->instruction) {
		case RET: opcode = 0b11001000; break;
		case UBR: opcode = 0b11000010; break;
		default: emitError(ERR_INTERNAL, NULL, "Could not encode instruction `%s`", INSTRUCTIONS[data->instruction]);
	}

	uint8_t rd = data->data.buType.xd->nodeData.reg->regNumber;

	encoding = (opcode << 24) | (rd << 0);

	return encoding;
}

static uint32_t encodeBc(InstrNode* data, uint32_t lp, SymbolTable* symbTable, RelocTable* relocTable) {
	initScope("encodeBc");

	uint32_t encoding = 0x00000000;
#ifdef _WIN64
	uint32_t opcode = 0b0000000;
#else
	uint8_t opcode = 0b0000000;
#endif

	switch (data->instruction) {
		case B: opcode = 0b11000100; break;
		default: emitError(ERR_INTERNAL, NULL, "Could not encode instruction `%s`", INSTRUCTIONS[data->instruction]);
	}

	Node* condNode = data->data.bcType.cond;
	uint8_t cond = condNode->nodeData.number->value.uint14Value;
	// cond should already be the numer representing the condition
	// ie if cond == 0, it represents eq

	// The rest is the same as Bi, mostly

	Node* labelNode = data->data.bcType.offset;

	
	RelData reldata = {
		.lp = lp,
		.addend = 0,
		.type = RELOC_TYPE_IR19,
		.relocTable = relocTable
	};
	int32_t label = (int32_t) getImmediateEncoding(labelNode, NTYPE_INT19, symbTable, &reldata);

	int32_t offset = 0x0;
	if (reldata.relocTable) {
		log("Label value for Bc-type instruction: 0x%X", label);

		log("LP value for Bc-type instruction: 0x%X", lp);

		offset = (label - lp) << 2;
		log("Computed offset for Bc-type instruction: 0x%X", offset);
	} else log("Bc-type instruction immediate resolved via relocation entry.");

	encoding = (opcode << 24) | (offset & 0x7FFFF) << 5 | (cond & 0x0F);
	log("Encoded Bc-type instruction `%s`: 0x%08X", INSTRUCTIONS[data->instruction], encoding);

	return encoding;
}

static uint32_t encodeBi(InstrNode* data, uint32_t lp, SymbolTable* symbTable, RelocTable* relocTable) {
	initScope("encodeBi");

	uint32_t encoding = 0x00000000;
#ifdef _WIN64
	uint32_t opcode = 0b0000000;
#else
	uint8_t opcode = 0b0000000;
#endif

	switch (data->instruction) {
		case UB: opcode = 0b11000000; break;
		case CALL: opcode = 0b11000110; break;
		default: emitError(ERR_INTERNAL, NULL, "Could not encode instruction `%s`", INSTRUCTIONS[data->instruction]);
	}

	Node* labelNode = data->data.biType.offset;
	
	RelData reldata = {
		.lp = lp,
		.addend = 0,
		.type = RELOC_TYPE_IR24,
		.relocTable = relocTable
	};
	int32_t label = (int32_t) getImmediateEncoding(labelNode, NTYPE_INT19, symbTable, &reldata);
	log("Label value for Bi-type instruction: 0x%X", label);

	int32_t offset = 0x0;
	if (reldata.relocTable) {
		// label is just the address to go to
		// It will be encoded as an offset from the current LP
		// It is then saved as the offset * 4 and signed extended to 19 bits

		log("LP value for Bi-type instruction: 0x%X", lp);

		offset = (label - lp) << 2;
		log("Computed offset for Bi-type instruction: 0x%X", offset);
	} else log("Bi-type instruction immediate resolved via relocation entry.");

	encoding = (opcode << 24) | ((offset & 0xFFFFFF));
	log("Encoded Bi-type instruction `%s`: 0x%08X", INSTRUCTIONS[data->instruction], encoding);

	return encoding;
}

static uint32_t encodeS(InstrNode* data) {
	initScope("encodeS");

	uint32_t encoding = 0x00000000;
#ifdef _WIN64
	uint32_t opcode = 0b10111110;
	uint32_t subOpcode = 0b000000000;
#else
	uint8_t opcode = 0b10111110;
	uint16_t subOpcode = 0b000000000;
#endif

	switch (data->instruction) {
		case SYSCALL: subOpcode = 0b000100000; break;
		case HLT: subOpcode = 0b001100000; break;
		case SI: subOpcode = 0b010100000; break;
		case DI: subOpcode = 0b011100000; break;
		case ERET: subOpcode = 0b100100000; break;
		case LDIR: subOpcode = 0b101100000; break;
		case MVCSTR: subOpcode = 0b110100000; break;
		case LDCSTR: subOpcode = 0b111100000; break;
		case RESR: subOpcode = 0b111110000; break;
		default: emitError(ERR_INTERNAL, NULL, "Could not encode instruction `%s`", INSTRUCTIONS[data->instruction]);
	}

	uint8_t rd = data->data.sType.xd ? data->data.sType.xd->nodeData.reg->regNumber : 0b00000;
	uint8_t rs = data->data.sType.xs ? data->data.sType.xs->nodeData.reg->regNumber : 0b00000;

	encoding = (opcode << 24) | (subOpcode << 15) | (rs << 5) | (rd << 0);

	return encoding;
}

static void gentext(Parser* parser, CodeGen* codegen, Node* ast) {
	initScope("gentext");

	enum InstrType type = ast->nodeData.instruction->instrType;

	uint32_t encoding = 0x00000000;

	uint32_t lp = 0x0;
	// Since instructions can be in text or in evt, the lp must change accordingly
	if (ast->nodeData.instruction->section == TEXT_SECT_N) lp = codegen->text.instructionCount * 4;
	else if (ast->nodeData.instruction->section == EVT_SECT_N) lp = codegen->evt.dataCount;

	/**
	 * Major note regarding LP in evt
	 * Since text and data are intertwined, there is a chance (by error of programmer)
	 *  that the instruction does not start in an address aligned to 4 bytes
	 * Structurally, all text is to be before the data, but....
	 * For example:
	 * ```
	 * .evt
	 * .byte 0x0
	 * ld x0, =_F0
	 * ```
	 * The instruction would begin at LP of 1.
	 * Since that is the beginning, the processor will read the first byte (0x0) and the following three (first three of the instruction)
	 *   as the instruction, leading to either the wrong instruction or a fault
	 */

	switch (type) {
		case I_TYPE: encoding = encodeI(ast->nodeData.instruction, lp, parser->symbolTable, parser->relocTable); break;
		case R_TYPE: encoding = encodeR(ast->nodeData.instruction); break;
		case M_TYPE: encoding = encodeM(ast->nodeData.instruction, lp, parser->symbolTable, parser->relocTable); break;
		case BU_TYPE: encoding = encodeBu(ast->nodeData.instruction); break;
		case BC_TYPE: encoding = encodeBc(ast->nodeData.instruction, lp, parser->symbolTable, parser->relocTable); break;
		case BI_TYPE: encoding = encodeBi(ast->nodeData.instruction, lp, parser->symbolTable, parser->relocTable); break;
		case S_TYPE: encoding = encodeS(ast->nodeData.instruction); break;
		default: break;
	}


	// Writing is different depending if it is to the instruction stream or the evt one
	// Since most importantly, the size is different

	if (ast->nodeData.instruction->section == TEXT_SECT_N) {
		log("Writing instruction to text section.");
		if (codegen->text.instructionCount == codegen->text.instructionCapacity) {
			codegen->text.instructionCapacity += 5;
			uint32_t* temp = (uint32_t*) realloc(codegen->text.instructions, codegen->text.instructionCapacity * sizeof(uint32_t));
			if (!temp) emitError(ERR_MEM, NULL, "Could not reallocate memory of instruction encodings.");
			// log("New instruction array: %p", temp);
			codegen->text.instructions = temp;
		}
		log("Writing 0x%x to index %d (address %p)", encoding, codegen->text.instructionCount, &codegen->text.instructions[codegen->text.instructionCount]);
		codegen->text.instructions[codegen->text.instructionCount] = encoding;
		log("Wrote 0x%x", codegen->text.instructions[codegen->text.instructionCount]);
		codegen->text.instructionCount++;
		return;
	}

	// Else, it is EVT section
	log("Writing instruction to evt section.");
	// Ensure enough capacity
	if (codegen->evt.dataCount + 4 >= codegen->evt.dataCapacity) {
		codegen->evt.dataCapacity += 5;
		uint8_t* temp = (uint8_t*) realloc(codegen->evt.data, codegen->evt.dataCapacity * sizeof(uint8_t));
		if (!temp) emitError(ERR_MEM, NULL, "Could not reallocate memory of evt data.");
		codegen->evt.data = temp;
	}
	log("Writing 0x%x to evt data at index %d (address %p)", encoding, codegen->evt.dataCount, &codegen->evt.data[codegen->evt.dataCount]);
	// Write the instruction as 4 bytes, little-endian
	codegen->evt.data[codegen->evt.dataCount + 0] = (uint8_t) ((encoding >> 0) & 0xFF);
	codegen->evt.data[codegen->evt.dataCount + 1] = (uint8_t) ((encoding >> 8) & 0xFF);
	codegen->evt.data[codegen->evt.dataCount + 2] = (uint8_t) ((encoding >> 16) & 0xFF);
	codegen->evt.data[codegen->evt.dataCount + 3] = (uint8_t) ((encoding >> 24) & 0xFF);
	codegen->evt.dataCount += 4;
}


/**
 * Generates data for a `.string` directive data entry.
 * @param codegen The codegen struct
 * @param entry The entry of the directive from the data table
 * @param entries The array of data entries of either data or const section
 * @param _entriesSize The size of the entries array
 * @param _entriesCapacity The capacity of the entries array
 * @param _idx The current index in the entries array
 * @param isData Whether the data is to be written to codegen data or const
 */
static void genString(CodeGen* codegen, data_entry_t* entry, data_entry_t** entries, int* _entriesSize, int* _entriesCapacity, int* _idx, sect_table_n section) {
	initScope("genString");

	// log("  Generating string data entry at address 0x%08X with size %d bytes.", entry->addr, entry->size);

	Node* stringNode = entry->data[0];
	// Make sure that the node is indeed a string node
	if (stringNode->nodeType != ND_STRING) emitError(ERR_INTERNAL, NULL, "Data entry for .string directive does not contain a string node.");

	uint8_t* codegenData= NULL;
	int* codegenDataCount = NULL;
	int* codegenDataCapacity = NULL;

	switch (section) {
		case DATA_SECT_N:
			codegenData = codegen->data.data;
			codegenDataCount = &codegen->data.dataCount;
			codegenDataCapacity = &codegen->data.dataCapacity;
			break;
		case CONST_SECT_N:
			codegenData = codegen->consts.data;
			codegenDataCount = &codegen->consts.dataCount;
			codegenDataCapacity = &codegen->consts.dataCapacity;
			break;
		default: return;
	}

	// Write the bytes (including null terminator) to the appropriate section
	for (int i = 0; i < entry->size; i++) {
		StrNode* strData = stringNode->nodeData.string;
		if (!strData) emitError(ERR_INTERNAL, NULL, "String node data is NULL.");

		// Ensure enough capacity
		if (*codegenDataCount == *codegenDataCapacity) {
			*codegenDataCapacity *= 2;
			uint8_t* temp = (uint8_t*) realloc(codegenData, sizeof(uint8_t) * (*codegenDataCapacity));
			if (!temp) emitError(ERR_MEM, NULL, "Failed to reallocate memory for data/const section.");
			codegenData = temp;
			if (section == DATA_SECT_N) codegen->data.data = codegenData;
			else codegen->consts.data = codegenData; // No check for const since at this point, it should have been checked that .string is not in evt
		}

		uint8_t byteValue = 0x00;
		if (i < strData->length) byteValue = (uint8_t) strData->value[i]; 
		else byteValue = 0x00; // Null terminator

		codegenData[*codegenDataCount] = byteValue;
		(*codegenDataCount)++;
		// log("    Wrote byte 0x%02X to %s section in codegen.", byteValue, isData ? "data" : "const");
	}
}

/**
 * Generates data for a `.bytes` directive data entry.
 * @param codegen The codegen struct
 * @param entry The entry of the directive from the data table
 * @param entries The array of data entries of either data or const section
 * @param _entriesSize The size of the entries array
 * @param _entriesCapacity The capacity of the entries array
 * @param _idx The current index in the entries array
 * @param isData Whether the data is to be written to codegen data or const
 */
static void genBytes(CodeGen* codegen, data_entry_t* entry, data_entry_t** entries, int* _entriesSize, int* _entriesCapacity, int* _idx, sect_table_n section) {
	initScope("genBytes");

	log("  Generating bytes data entry at address 0x%08X with size %d bytes.", entry->addr, entry->size);

	linedata_ctx linedata = {
		.linenum = entry->linenum,
		.source = ssGetString(entry->source)
	};

	uint8_t* codegenData= NULL;
	int* codegenDataCount = NULL;
	int* codegenDataCapacity = NULL;

	switch (section) {
		case DATA_SECT_N:
			codegenData = codegen->data.data;
			codegenDataCount = &codegen->data.dataCount;
			codegenDataCapacity = &codegen->data.dataCapacity;
			break;
		case CONST_SECT_N:
			codegenData = codegen->consts.data;
			codegenDataCount = &codegen->consts.dataCount;
			codegenDataCapacity = &codegen->consts.dataCapacity;
			break;
		case EVT_SECT_N:
			codegenData = codegen->evt.data;
			codegenDataCount = &codegen->evt.dataCount;
			codegenDataCapacity = &codegen->evt.dataCapacity;
			break;
		default: return;
	}

	// Write the bytes to the appropriate section
	for (int i = 0; i < entry->size; i++) {
		// Ensure enough capacity
		if (*codegenDataCount == *codegenDataCapacity) {
			*codegenDataCapacity *= 2;
			uint8_t* temp = (uint8_t*) realloc(codegenData, sizeof(uint8_t) * (*codegenDataCapacity));
			if (!temp) emitError(ERR_MEM, NULL, "Failed to reallocate memory for data/const section.");
			codegenData = temp;
			if (section == DATA_SECT_N) codegen->data.data = codegenData;
			else if (section == CONST_SECT_N) codegen->consts.data = codegenData;
			else if (section == EVT_SECT_N) codegen->evt.data = codegenData;
		}

		Node* byteExpr = entry->data[i];

		// Need to evaluate the expression
		bool evald = evaluateExpression(byteExpr, codegen->symbolTable);
		// TODO: have relocations for extern
		if (!evald) emitError(ERR_INVALID_EXPRESSION, &linedata, "Could not evaluate immediate expression.");

		// The byteExpr node can either be a number itself, an operator, or a symbol, so get its value accordingly
		uint8_t byteValue = 0x00;
		switch (byteExpr->nodeType) {
			case ND_NUMBER: {
				NumNode* numData = byteExpr->nodeData.number;
				if (!numData) emitError(ERR_INTERNAL, NULL, "Number node data is NULL.");
				// Check type
				if (numData->type > NTYPE_INT8) {
					emitError(ERR_INVALID_TYPE, &linedata, "Data entry number node is not of byte type.");
				}
				byteValue = (uint8_t) numData->value.int8Value; // Just take the lowest byte
				break;
			}
			case ND_OPERATOR: {
				OpNode* opData = (OpNode*) byteExpr->nodeData.operator;
				if (!opData) emitError(ERR_INTERNAL, NULL, "Operator node data is NULL.");
				// Check type
				if (opData->valueType > NTYPE_INT8) {
					emitError(ERR_INVALID_TYPE, &linedata, "Data entry operator node is not of byte type.");
				}
				byteValue = (uint8_t) opData->value; // Just take the lowest byte
				break;
			}
			case ND_SYMB: {
				SymbNode* symbData = (SymbNode*) byteExpr->nodeData.symbol;
				if (!symbData) emitError(ERR_INTERNAL, NULL, "Symbol node data is NULL.");
				int idx = symbData->symbTableIndex;
				if (idx < 0 || idx >= (int)codegen->symbolTable->size) {
					emitError(ERR_INTERNAL, NULL, "Symbol index %d out of bounds in symbol table.", idx);
				}
				symb_entry_t* symbEntry = codegen->symbolTable->entries[idx];
				if (!symbEntry) emitError(ERR_INTERNAL, NULL, "Symbol table entry at index %d is NULL.", idx);

				byteValue = (uint8_t) symbEntry->value.val; // Just take the lowest byte
				break;
			}
			default:
				emitError(ERR_INTERNAL, &linedata, "Data entry expression is of invalid type.");
		}

		codegenData[*codegenDataCount] = byteValue;
		(*codegenDataCount)++;
		log("    Wrote byte 0x%02X to %s section in codegen.", byteValue, (section == DATA_SECT_N) ? "data" : (section == CONST_SECT_N) ? "const" : "evt");
	}
}

static void genHwords(CodeGen* codegen, data_entry_t* entry, data_entry_t** entries, int* _entriesSize, int* _entriesCapacity, int* _idx, sect_table_n section) {
	initScope("genHwords");

	log("  Generating halfword data entry at address 0x%08X with size %d bytes.", entry->addr, entry->size);

	linedata_ctx linedata = {
		.linenum = entry->linenum,
		.source = ssGetString(entry->source)
	};

	uint8_t* codegenData= NULL;
	int* codegenDataCount = NULL;
	int* codegenDataCapacity = NULL;

	switch (section) {
		case DATA_SECT_N:
			codegenData = codegen->data.data;
			codegenDataCount = &codegen->data.dataCount;
			codegenDataCapacity = &codegen->data.dataCapacity;
			break;
		case CONST_SECT_N:
			codegenData = codegen->consts.data;
			codegenDataCount = &codegen->consts.dataCount;
			codegenDataCapacity = &codegen->consts.dataCapacity;
			break;
		case EVT_SECT_N:
			codegenData = codegen->evt.data;
			codegenDataCount = &codegen->evt.dataCount;
			codegenDataCapacity = &codegen->evt.dataCapacity;
			break;
		default: return;
	}

	// Write the halfwords to the appropriate section
	for (int i = 0; i < entry->size / 2; i++) {
		Node* hwordExpr = entry->data[i];

		// Need to evaluate the expression
		bool evald = evaluateExpression(hwordExpr, codegen->symbolTable);
		// TODO: have relocations for extern
		if (!evald) emitError(ERR_INVALID_EXPRESSION, &linedata, "Could not evaluate immediate expression.");

		// The hwordExpr node can either be a number itself, an operator, or a symbol, so get its value accordingly
		uint16_t hwordValue = 0x0000;
		switch (hwordExpr->nodeType) {
			case ND_NUMBER: {
				NumNode* numData = hwordExpr->nodeData.number;
				if (!numData) emitError(ERR_INTERNAL, NULL, "Number node data is NULL.");
				// Check type
				if (numData->type > NTYPE_INT16) {
					emitError(ERR_INVALID_TYPE, &linedata, "Data entry number node is not of halfword type.");
				}
				hwordValue = (uint16_t) numData->value.int16Value; // Just take the lowest 2 bytes
				break;
			}
			case ND_OPERATOR: {
				OpNode* opData = (OpNode*) hwordExpr->nodeData.operator;
				if (!opData) emitError(ERR_INTERNAL, NULL, "Operator node data is NULL.");
				// Check type
				if (opData->valueType > NTYPE_INT16) {
					emitError(ERR_INVALID_TYPE, &linedata, "Data entry operator node is not of halfword type.");
				}
				hwordValue = (uint16_t) opData->value; // Just take the lowest 2 bytes
				break;
			}
			case ND_SYMB: {
				SymbNode* symbData = (SymbNode*) hwordExpr->nodeData.symbol;
				if (!symbData) emitError(ERR_INTERNAL, NULL, "Symbol node data is NULL.");
				int idx = symbData->symbTableIndex;
				if (idx < 0 || idx >= (int)codegen->symbolTable->size) {
					emitError(ERR_INTERNAL, NULL, "Symbol index %d out of bounds in symbol table.", idx);
				}
				symb_entry_t* symbEntry = codegen->symbolTable->entries[idx];
				if (!symbEntry) emitError(ERR_INTERNAL, NULL, "Symbol table entry at index %d is NULL.", idx);

				hwordValue = (uint16_t) symbEntry->value.val; // Just take the lowest 2 bytes
				break;
			}
			default:
				emitError(ERR_INTERNAL, &linedata, "Data entry expression is of invalid type.");
		}
		// Write the halfword in little-endian
		// Ensure enough capacity for 2 bytes
		for (int b = 0; b < 2; b++) {
			if (*codegenDataCount == *codegenDataCapacity) {
				*codegenDataCapacity *= 2;
				uint8_t* temp = (uint8_t*) realloc(codegenData, sizeof(uint8_t) * (*codegenDataCapacity));
				if (!temp) emitError(ERR_MEM, NULL, "Failed to reallocate memory for data/const section.");
				codegenData = temp;

				if (section == DATA_SECT_N) codegen->data.data = codegenData;
				else if (section == CONST_SECT_N) codegen->consts.data = codegenData;
				else if (section == EVT_SECT_N) codegen->evt.data = codegenData;
			}

			codegenData[*codegenDataCount] = (hwordValue >> (8 * b)) & 0xFF;
			(*codegenDataCount)++;
		}
		log("    Wrote halfword 0x%04X to %s section in codegen.", hwordValue, (section == DATA_SECT_N) ? "data" : (section == CONST_SECT_N) ? "const" : "evt");
	}
}

static void genWords(CodeGen* codegen, data_entry_t* entry, data_entry_t** entries, int* _entriesSize, int* _entriesCapacity, int* _idx, sect_table_n section) {
	initScope("genWords");

	log("  Generating word data entry at address 0x%08X with size %d bytes.", entry->addr, entry->size);

	linedata_ctx linedata = {
		.linenum = entry->linenum,
		.source = ssGetString(entry->source)
	};

	uint8_t* codegenData= NULL;
	int* codegenDataCount = NULL;
	int* codegenDataCapacity = NULL;

	switch (section) {
		case DATA_SECT_N:
			codegenData = codegen->data.data;
			codegenDataCount = &codegen->data.dataCount;
			codegenDataCapacity = &codegen->data.dataCapacity;
			break;
		case CONST_SECT_N:
			codegenData = codegen->consts.data;
			codegenDataCount = &codegen->consts.dataCount;
			codegenDataCapacity = &codegen->consts.dataCapacity;
			break;
		case EVT_SECT_N:
			codegenData = codegen->evt.data;
			codegenDataCount = &codegen->evt.dataCount;
			codegenDataCapacity = &codegen->evt.dataCapacity;
			break;
		default: return;
	}

	// Write the words to the appropriate section
	for (int i = 0; i < entry->size / 4; i++) {
		Node* wordExpr = entry->data[i];

		// Need to evaluate the expression
		bool evald = evaluateExpression(wordExpr, codegen->symbolTable);
		// TODO: have relocations for extern
		if (!evald) emitError(ERR_INVALID_EXPRESSION, &linedata, "Could not evaluate immediate expression.");

		// The wordExpr node can either be a number itself, an operator, or a symbol, so get its value accordingly
		// Note that in the case it uses a symbol, there is a chance that symbol is an address
		// That is the case to do a relocation
		uint32_t wordValue = 0x00000000;
		switch (wordExpr->nodeType) {
			case ND_NUMBER: {
				NumNode* numData = wordExpr->nodeData.number;
				if (!numData) emitError(ERR_INTERNAL, NULL, "Number node data is NULL.");
				// Check type, not really needed since max it can go is int32 but maybe in the future for aru64
				if (numData->type > NTYPE_INT32) {
					emitError(ERR_INVALID_TYPE, &linedata, "Data entry number node is not of word type.");
				}
				wordValue = numData->value.uint32Value;
				break;
			}
			case ND_OPERATOR: {
				OpNode* opData = (OpNode*) wordExpr->nodeData.operator;
				if (!opData) emitError(ERR_INTERNAL, NULL, "Operator node data is NULL.");
				// Check type, not really needed since max it can go is int32 but maybe in the future for aru64
				if (opData->valueType > NTYPE_INT32) {
					emitError(ERR_INVALID_TYPE, &linedata, "Data entry operator node is not of word type.");
				}
				wordValue = opData->value;
				break;
			}
			case ND_SYMB: {
				SymbNode* symbData = (SymbNode*) wordExpr->nodeData.symbol;
				if (!symbData) emitError(ERR_INTERNAL, NULL, "Symbol node data is NULL.");
				int idx = symbData->symbTableIndex;
				if (idx < 0 || idx >= (int)codegen->symbolTable->size) {
					emitError(ERR_INTERNAL, NULL, "Symbol index %d out of bounds in symbol table.", idx);
				}
				symb_entry_t* symbEntry = codegen->symbolTable->entries[idx];
				if (!symbEntry) emitError(ERR_INTERNAL, NULL, "Symbol table entry at index %d is NULL.", idx);

				if (GET_MAIN_TYPE(symbEntry->flags) != M_ABS) {
					// The symbol is an address, have a relocation entry
					// The symbol can only be relocated if it is a simple +/- expression
					Node* symb = getExternSymbol(wordExpr);
					if (!symb) {
						linedata_ctx linedata = {
							.linenum = wordExpr->token->linenum,
							.source = ssGetString(wordExpr->token->sstring)
						};
						emitError(ERR_INVALID_EXPRESSION, &linedata, "Invalid expression for relocation.");
					}

					int32_t addend = 0;
					// Get the addend
					if (wordExpr->nodeType == ND_OPERATOR) {
						if (wordExpr->nodeData.operator->data.binary.left->nodeType == ND_NUMBER) {
							addend = wordExpr->nodeData.operator->data.binary.left->nodeData.number->value.int32Value;
						} else if (wordExpr->nodeData.operator->data.binary.right->nodeType == ND_NUMBER) {
							addend = wordExpr->nodeData.operator->data.binary.right->nodeData.number->value.int32Value;
						} else {
							emitError(ERR_INTERNAL, &linedata, "Failed to find number node for addend in immediate.");
						}
					}

					RelocEnt* reloc = initRelocEntry(entry->addr, idx, RELOC_TYPE_WORD, addend);
					addRelocEntry(codegen->relocTable, section, reloc);
				}

				wordValue = symbEntry->value.val;
				break;
			}
			default:
				emitError(ERR_INTERNAL, &linedata, "Data entry expression is of invalid type.");
		}
		// Write the word in little-endian
		// Ensure enough capacity for 4 bytes
		for (int b = 0; b < 4; b++) {
			if (*codegenDataCount == *codegenDataCapacity) {
				*codegenDataCapacity *= 2;
				uint8_t* temp = (uint8_t*) realloc(codegenData, sizeof(uint8_t) * (*codegenDataCapacity));
				if (!temp) emitError(ERR_MEM, NULL, "Failed to reallocate memory for data/const section.");
				codegenData = temp;
				if (section == DATA_SECT_N) codegen->data.data = codegenData;
				else if (section == CONST_SECT_N) codegen->consts.data = codegenData;
				else if (section == EVT_SECT_N) codegen->evt.data = codegenData;
			}
			codegenData[*codegenDataCount] = (wordValue >> (8 * b)) & 0xFF;
			(*codegenDataCount)++;
		}
		log("    Wrote word 0x%08X to %s section in codegen.", wordValue, (section == DATA_SECT_N) ? "data" : (section == CONST_SECT_N) ? "const" : "evt");
	}
}

static void genFloats(CodeGen* codegen, data_entry_t* entry, data_entry_t** entries, int* _entriesSize, int* _entriesCapacity, int* _idx, sect_table_n section) {
	initScope("genFloats");

	log("  Generating float data entry at address 0x%08X with size %d bytes.", entry->addr, entry->size);

	linedata_ctx linedata = {
		.linenum = entry->linenum,
		.source = ssGetString(entry->source)
	};

	uint8_t* codegenData= NULL;
	int* codegenDataCount = NULL;
	int* codegenDataCapacity = NULL;

	switch (section) {
		case DATA_SECT_N:
			codegenData = codegen->data.data;
			codegenDataCount = &codegen->data.dataCount;
			codegenDataCapacity = &codegen->data.dataCapacity;
			break;
		case CONST_SECT_N:
			codegenData = codegen->consts.data;
			codegenDataCount = &codegen->consts.dataCount;
			codegenDataCapacity = &codegen->consts.dataCapacity;
			break;
		default: return;
	}

	// Write the floats to the appropriate section
	for (int i = 0; i < entry->size / 4; i++) {
		Node* floatExpr = entry->data[i];

		// Need to evaluate the expression
		bool evald = evaluateExpression(floatExpr, codegen->symbolTable);
		// TODO: have relocations for externs??
		if (!evald) emitError(ERR_INVALID_EXPRESSION, &linedata, "Could not evaluate immediate expression.");

		// The floatExpr node can either be a number itself, an operator, or a symbol, so get its value accordingly
		float floatValue = 0.0f;
		switch (floatExpr->nodeType) {
			case ND_NUMBER: {
				NumNode* numData = floatExpr->nodeData.number;
				if (!numData) emitError(ERR_INTERNAL, NULL, "Number node data is NULL.");
				// Check type
				if (numData->type != NTYPE_FLOAT) {
					emitError(ERR_INVALID_TYPE, &linedata, "Data entry number node is not of float type.");
				}
				floatValue = numData->value.floatValue;
				break;
			}
			case ND_OPERATOR: {
				OpNode* opData = (OpNode*) floatExpr->nodeData.operator;
				if (!opData) emitError(ERR_INTERNAL, NULL, "Operator node data is NULL.");
				// Check type
				if (opData->valueType != NTYPE_FLOAT) {
					emitError(ERR_INVALID_TYPE, &linedata, "Data entry operator node is not of float type.");
				}
				floatValue = (float) opData->value;
				break;
			}
			case ND_SYMB: {
				SymbNode* symbData = (SymbNode*) floatExpr->nodeData.symbol;
				if (!symbData) emitError(ERR_INTERNAL, NULL, "Symbol node data is NULL.");
				int idx = symbData->symbTableIndex;
				if (idx < 0 || idx >= (int)codegen->symbolTable->size) {
					emitError(ERR_INTERNAL, NULL, "Symbol index %d out of bounds in symbol table.", idx);
				}
				symb_entry_t* entry = codegen->symbolTable->entries[idx];
				if (!entry) {
					emitError(ERR_INTERNAL, NULL, "Symbol table entry at index %d is NULL.", idx);
				}
				// if (!GET_DEFINED(entry->flags)) {
				// 	// This should have been taken care of by eval????
				// 	emitError(ERR_UNDEFINED, &linedata, "Symbol `%s` is not defined.", entry->name);
				// }
				floatValue = (float) entry->value.val;
				break;
			}
			default:
				emitError(ERR_INTERNAL, &linedata, "Data entry expression is of invalid type.");
		}
		// Write the float in little-endian
		uint32_t floatAsInt = 0;
		memcpy(&floatAsInt, &floatValue, sizeof(float));
		// Ensure enough capacity for 4 bytes
		for (int b = 0; b < 4; b++) {
			if (*codegenDataCount == *codegenDataCapacity) {
				*codegenDataCapacity *= 2;
				uint8_t* temp = (uint8_t*) realloc(codegenData, sizeof(uint8_t) * (*codegenDataCapacity));
				if (!temp) emitError(ERR_MEM, NULL, "Failed to reallocate memory for data/const section.");
				codegenData = temp;
				if (section == DATA_SECT_N) codegen->data.data = codegenData;
				else codegen->consts.data = codegenData; // Same as string, float does not happen in evt
			}
			codegenData[*codegenDataCount] = (floatAsInt >> (8 * b)) & 0xFF;
			(*codegenDataCount)++;
		}
		log("    Wrote float %f to %s section in codegen.", floatValue, (section == DATA_SECT_N) ? "data" : "const");
	}
}

static void genZeros(CodeGen* codegen, data_entry_t* entry, data_entry_t** entries, int* _entriesSize, int* _entriesCapacity, int* _idx, sect_table_n section) {
	initScope("genZeros");

	log("  Generating zero/fill data entry at address 0x%08X with size %d bytes.", entry->addr, entry->size);

	uint8_t* codegenData= NULL;
	int* codegenDataCount = NULL;
	int* codegenDataCapacity = NULL;

	switch (section) {
		case DATA_SECT_N:
			codegenData = codegen->data.data;
			codegenDataCount = &codegen->data.dataCount;
			codegenDataCapacity = &codegen->data.dataCapacity;
			break;
		case CONST_SECT_N:
			codegenData = codegen->consts.data;
			codegenDataCount = &codegen->consts.dataCount;
			codegenDataCapacity = &codegen->consts.dataCapacity;
			break;
		case EVT_SECT_N:
			codegenData = codegen->evt.data;
			codegenDataCount = &codegen->evt.dataCount;
			codegenDataCapacity = &codegen->evt.dataCapacity;
			break;
		default: return;
	}

	// Write zeros to the appropriate section
	for (int i = 0; i < entry->size; i++) {
		// Ensure enough capacity
		if (*codegenDataCount == *codegenDataCapacity) {
			*codegenDataCapacity *= 2;
			uint8_t* temp = (uint8_t*) realloc(codegenData, sizeof(uint8_t) * (*codegenDataCapacity));
			if (!temp) emitError(ERR_MEM, NULL, "Failed to reallocate memory for data/const section.");
			codegenData = temp;
			if (section == DATA_SECT_N) codegen->data.data = codegenData;
			else if (section == CONST_SECT_N) codegen->consts.data = codegenData;
			else if (section == EVT_SECT_N) codegen->evt.data = codegenData;
		}

		codegenData[*codegenDataCount] = 0x00;
		(*codegenDataCount)++;
		// log("    Wrote zero byte to %s section in codegen.", (section == DATA_SECT_N) ? "data" : (section == CONST_SECT_N) ? "const" : "evt");
	}
}

static void genFill(CodeGen* codegen, data_entry_t* entry, data_entry_t** entries, int* _entriesSize, int* _entriesCapacity, int* _idx, sect_table_n section) {
	initScope("genFill");

	log("  Generating fill data entry at address 0x%08X with size %d bytes.", entry->addr, entry->size);

	uint8_t* codegenData= NULL;
	int* codegenDataCount = NULL;
	int* codegenDataCapacity = NULL;

	switch (section) {
		case DATA_SECT_N:
			codegenData = codegen->data.data;
			codegenDataCount = &codegen->data.dataCount;
			codegenDataCapacity = &codegen->data.dataCapacity;
			break;
		case CONST_SECT_N:
			codegenData = codegen->consts.data;
			codegenDataCount = &codegen->consts.dataCount;
			codegenDataCapacity = &codegen->consts.dataCapacity;
			break;
		case EVT_SECT_N:
			codegenData = codegen->evt.data;
			codegenDataCount = &codegen->evt.dataCount;
			codegenDataCapacity = &codegen->evt.dataCapacity;
			break;
		default: return;
	}

	// Write fill bytes to the appropriate section
	// In handleFill, the data was set as an array of two with [0] as len and [1] as num
	// Make sure this is true
	if (entry->dataCount != 2) emitError(ERR_INTERNAL, NULL, "Fill data entry does not have exactly two data nodes.");
	if (!entry->data[1]) emitError(ERR_INTERNAL, NULL, "Fill data entry's fill byte node is NULL.");

	uint8_t fillByte = (uint8_t) entry->data[1]->nodeData.number->value.int8Value;

	for (int i = 0; i < entry->size; i++) {
		// Ensure enough capacity
		if (*codegenDataCount == *codegenDataCapacity) {
			*codegenDataCapacity *= 2;
			uint8_t* temp = (uint8_t*) realloc(codegenData, sizeof(uint8_t) * (*codegenDataCapacity));
			if (!temp) emitError(ERR_MEM, NULL, "Failed to reallocate memory for data/const section.");
			codegenData = temp;
			if (section == DATA_SECT_N) codegen->data.data = codegenData;
			else if (section == CONST_SECT_N) codegen->consts.data = codegenData;
			else if (section == EVT_SECT_N) codegen->evt.data = codegenData;
		}

		codegenData[*codegenDataCount] = fillByte;
		(*codegenDataCount)++;
		// log("    Wrote fill byte 0x%02X to %s section in codegen.", fillByte, (section == DATA_SECT_N) ? "data" : (section == CONST_SECT_N) ? "const" : "evt");
	}
}

static void gendata(Parser* parser, Node* ast, CodeGen* codegen, int* dataIdx, int* constIdx, int* evtIdx) {
	initScope("gendata");

	sect_table_n section = ast->nodeData.directive->section;

	data_entry_t** entries = NULL;
	int* entriesSize = NULL;
	int* entriesCapacity = NULL;
	int* idx = NULL;

	switch (section) {
		case DATA_SECT_N:
			entries = parser->dataTable->dataEntries;
			entriesSize = (int*)&parser->dataTable->dSize;
			entriesCapacity = (int*)&parser->dataTable->dCapacity;
			idx = dataIdx;
			break;
		case CONST_SECT_N:
			entries = parser->dataTable->constEntries;
			entriesSize = (int*)&parser->dataTable->cSize;
			entriesCapacity = (int*)&parser->dataTable->cCapacity;
			idx = constIdx;
			break;
		case BSS_SECT_N: return; // No data to generate for BSS
		case EVT_SECT_N:
			entries = parser->dataTable->evtEntries;
			entriesSize = (int*)&parser->dataTable->eSize;
			entriesCapacity = (int*)&parser->dataTable->eCapacity;
			idx = evtIdx;
			break;
		case IVT_SECT_N:
			emitWarning(WARN_UNIMPLEMENTED, NULL, "Data generation for IVT section not yet implemented.");
			return;
		default:
			emitError(ERR_INTERNAL, NULL, "Data generation in invalid section %d.", section);
			break;
	}

	/**
	 * Very important note here:
	 * It may seem that there is a disconnect between using an index on the data entries and the current ast node
	 * As in will `entry` really be the data entry that `ast` refers to?
	 * The answer is yes (pretty sure I think)
	 * The codegen loops through the ASTs in the order that they were created
	 * The data entries also get created in the order of the data directives
	 * For example, line 5 contains the first data directive, creating the first data entry
	 * Line 10 has the next data directive, creating the second data entry
	 * On codegen, when it arrives to the line 5-first data directive AST node, the index is 0
	 * Thus it uses the first data entry, advancing to 1
	 * When the next data directive AST node arrives, the index is 1, accessing its respective data entry
	 * That also means no need to pass in `ast` as the entry already contains a pointer to it
	 * `ast` was only needed to get the section
	 */

	data_entry_t* entry = entries[*idx];
	if (!entry) emitError(ERR_INTERNAL, NULL, "Data entry at index %d is NULL.", *idx);

	log("  Generating data entry %d of type %d at address 0x%08X with size %d bytes.", *idx, entry->type, entry->addr, entry->size);

	// Intercept zero and fill since those have a difference structure
	// Especially .zero
	// There is a chance that it is in a non-bss section
	// And converting the simple AST (.zero -> size) to x nodes is too much for just zeros
	// Maybe it is worth it for .fill, but in the case that the number is 0, it might apply the same as .zero

	if (ast->token->type == TK_D_ZERO) {
		genZeros(codegen, entry, entries, entriesSize, entriesCapacity, idx, section);
		(*idx)++;
		return;
	} else if (ast->token->type == TK_D_FILL) {
		genFill(codegen, entry, entries, entriesSize, entriesCapacity, idx, section);
		(*idx)++;
		return;
	}

	// Depending on the type of the data entry
	switch (entry->type) {
		case STRING_TYPE: genString(codegen, entry, entries, entriesSize, entriesCapacity, idx, section); break;
		case BYTES_TYPE: genBytes(codegen, entry, entries, entriesSize, entriesCapacity, idx, section); break;
		case HWORDS_TYPE: genHwords(codegen, entry, entries, entriesSize, entriesCapacity, idx, section); break;
		case WORDS_TYPE: genWords(codegen, entry, entries, entriesSize, entriesCapacity, idx, section); break;
		case FLOATS_TYPE: genFloats(codegen, entry, entries, entriesSize, entriesCapacity, idx, section); break;
		default: emitError(ERR_INTERNAL, NULL, "Data entry type %d invalid", entry->type);
	}
	(*idx)++;
}


/**
 * Checks for any defined but unused symbols and evaluating them to ensure they are expression-less.
 * @param symbTable 
 */
static void resolveSymbols(SymbolTable* symbTable) {
	initScope("resolveSymbols");

	for (int i = 0; i < symbTable->size; i++) {
		symb_entry_t* entry = symbTable->entries[i];

		// If the symbol is defined but not used, check if it is expression-less
		// Note that since externs are set as undef, it is fine
		if (GET_DEFINED(entry->flags) == D_DEF && GET_REFERENCED(entry->flags) == R_NREF && GET_MAIN_TYPE(entry->flags) == M_ABS) {
			linedata_ctx linedata = {
				.linenum = entry->linenum,
				.source = ssGetString(entry->source)
			};

			emitWarning(WARN_UNUSED, NULL, "Symbol `%s` defined at `%s` but not used.", entry->name, linedata.source);

			log("Resolving defined but unused symbol `%s` at index %d.", entry->name, i);
			// Evaluate the symbol's expression
			bool evald = evaluateExpression(entry->value.expr, symbTable);
			if (!evald) {
				emitError(ERR_INVALID_EXPRESSION, &linedata, "Could not evaluate expression for defined but unused symbol `%s`.", entry->name);
			}
			
			uint32_t val = 0x0;
			if (entry->value.expr->nodeType == ND_NUMBER) val = entry->value.expr->nodeData.number->value.uint32Value;
			else if (entry->value.expr->nodeType == ND_SYMB) val = entry->value.expr->nodeData.symbol->value;
			else if (entry->value.expr->nodeType == ND_OPERATOR) val = entry->value.expr->nodeData.operator->value;

			entry->value.expr = NULL;
			CLR_EXPRESSION(entry->flags);
			entry->value.val = val;
			log("  Symbol `%s` resolved to value 0x%08X.", entry->name, entry->value.val);

		}
	}
}

void gencode(Parser* parser, CodeGen* codegen) {
	initScope("gencode");

	int dataIdx = 0;
	int constIdx = 0;
	int evtIdx = 0;
	for (int i = 0; i < parser->astCount; i++) {
		Node* ast = parser->asts[i];
		log("Generating code for AST %d (%p):", i, ast);

		// linedata_ctx linedata = {
		// 	.linenum = ast->token->linenum,
		// 	.source = ssGetString(ast->token->sstring)
		// };

		// Each ast root will be either a label, instruction, or directive
		// Labels can be ignored

		switch (ast->nodeType) {
			case ND_INSTRUCTION:
				log("  Instruction");
				// Quick intervention
				// In the case that the ast is an LD instruction
				// And the LD is LD imm/move, the text to generate is not the LD instruction itself but the decomposed ones in `expanded`
				if (ast->nodeData.instruction->instruction == LD && !ast->nodeData.instruction->data.mType.xb) {
					for (int i = 0; i < 6; i++) {
						Node* expandedInstr = ast->nodeData.instruction->data.mType.expanded[i];
						log("    Generating expanded instruction %d for LD immediate/move form:", i);
						gentext(parser, codegen, expandedInstr);
					}

					if (ast->nodeData.instruction->data.mType.expanded[6]) {
						log("    Generating expanded instruction 6 for LD immediate form:");
						gentext(parser, codegen, ast->nodeData.instruction->data.mType.expanded[6]);
					}
				} else gentext(parser, codegen, ast);
				break;
			case ND_DIRECTIVE:
				// log("  Directive");
				// Certain directives are useless
				// The directives to care about all the data ones
				if (ast->token->type < TK_D_STRING || ast->token->type > TK_D_ALIGN) {
					log("    Ignoring directive %s", ast->token->lexeme);
					break;
				}

				// Data directives are stored in the data table
				log("    Processing directive %s", ast->token->lexeme);
				gendata(parser, ast, codegen, &dataIdx, &constIdx, &evtIdx);
				break;
			default:
				log("  AST root is neither instruction nor directive, ignoring.");
				break;
		}
	}

	resolveSymbols(parser->symbolTable);
}


void displayCodeGen(CodeGen* codegen) {
	rlog("CodeGen State:");

	rlog("Text Section: %d instructions", codegen->text.instructionCount);
	for (int i = 0; i < codegen->text.instructionCount; i++) {
		rlog("  [%04d] 0x%08X", i*4, codegen->text.instructions[i]);
	}

	rlog("Data Section: %d bytes", codegen->data.dataCount);
	for (int i = 0; i < codegen->data.dataCount; i++) {
		if (i % 16 == 0) rlog("  [%04d] ", i);
		rlog("%02X ", codegen->data.data[i]);
		if (i % 16 == 15 || i == codegen->data.dataCount - 1) rlog("\n");
	}

	rlog("Const Section: %d bytes", codegen->consts.dataCount);
	for (int i = 0; i < codegen->consts.dataCount; i++) {
		if (i % 16 == 0) rlog("  [%04d] ", i);
		rlog("%02X ", codegen->consts.data[i]);
		if (i % 16 == 15 || i == codegen->consts.dataCount - 1) rlog("\n");
	}
}