#include "codegen.h"
#include "expr.h"
#include "diagnostics.h"


CodeGen* initCodeGenerator(SectionTable* sectionTable, SymbolTable* symbolTable) {
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

	codegen->sectionTable = sectionTable;
	codegen->symbolTable = symbolTable;

	return codegen;
}

void deinitCodeGenerator(CodeGen* codegen) {	
	free(codegen->text.instructions);
	free(codegen->data.data);
	free(codegen->consts.data);
	free(codegen);
}

static uint32_t getImmediateEncoding(Node* immNode, NumType expectedType, SymbolTable* symbTable) {
	initScope("getImmediateEncoding");

	log("Getting immediate encoding for %s", immNode->token->lexeme);
	printAST(immNode);
	bool evald = evaluateExpression(immNode, symbTable);
	linedata_ctx linedata = {
		.linenum = immNode->token->linenum,
		.source = ssGetString(immNode->token->sstring)
	};
	if (!evald) emitError(ERR_INVALID_EXPRESSION, &linedata, "Could not evaluate immediate expression.");


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
			if (idx < 0 || idx >= (int)symbTable->size) {
				emitError(ERR_INTERNAL, NULL, "Symbol index %d out of bounds in symbol table.", idx);
			}
			symb_entry_t* entry = symbTable->entries[idx];
			if (!entry) {
				emitError(ERR_INTERNAL, NULL, "Symbol table entry at index %d is NULL.", idx);
			}
			if (!GET_DEFINED(entry->flags)) {
				emitError(ERR_UNDEFINED, &linedata, "Symbol `%s` is not defined.", entry->name);
			}
			value = entry->value.val; // Use val for now
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
	return value;
}


static uint32_t encodeI(InstrNode* data, SymbolTable* symbTable) {
	initScope("encodeI");

	uint32_t encoding = 0x00000000;
#ifdef _WIN64
	uint32_t opcode = 0b0000000;
#else
	uint8_t opcode = 0b0000000;
#endif

	switch (data->instruction) {
		case ADD: case MV: case NOP: opcode = 0b10000000; break;
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

	if (immNode) {
		imm14 = (uint16_t) getImmediateEncoding(immNode, NTYPE_UINT14, symbTable);
	} else {
		// For security, check that the instruction is in fact NOP
		if (data->instruction != NOP) emitError(ERR_INTERNAL, NULL, "Immediate node is NULL for non-NOP instruction.");
	}

	encoding = (opcode << 24) | (imm14 << 10) | (rs << 5) | (rd << 0);

	return encoding;
}

static uint32_t encodeR(InstrNode* data) {
	initScope("encodeR");

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
		case ADD: case MV: opcode = 0b10000001; break;
		case ADDS: opcode = 0b10001001; break;
		case SUB: case MVN: opcode = 0b10010001; break;
		case SUBS: case CMP: opcode = 0b10011001; break;
		case OR: opcode = 0b01000001; break;
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

	return encoding;
}

static uint32_t encodeM(InstrNode* data, SymbolTable* symbTable) {
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
	// rs is the optional base register
	// It does not exist when the instruction is LD immediate form
	uint8_t rs = data->data.mType.xb ? data->data.mType.xb->nodeData.reg->regNumber : 0b00000;
	// Ensure that for all other forms other than LD immediate, rs is not null
	if (data->instruction != LD) {
		if (!data->data.mType.xb) {
			emitError(ERR_INTERNAL, NULL, "Base register is NULL for non-LD immediate instruction.");
		}
	}

	// rr is the optional index register, defaults to 0b00000
	uint8_t rr = data->data.mType.xi ? data->data.mType.xi->nodeData.reg->regNumber : 0b00000;

	Node* immNode = data->data.mType.imm;

	// imm is optional
	int16_t imm9 = 0x0000;

	if (immNode) imm9 = (int16_t) getImmediateEncoding(immNode, NTYPE_INT9, symbTable);

	encoding = (opcode << 24) | ((imm9 & 0x1FF) << 15) | (rr << 10) | (rs << 5) | (rd << 0);

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

	switch (type) {
		case I_TYPE: encoding = encodeI(ast->nodeData.instruction, parser->symbolTable); break;
		case R_TYPE: encoding = encodeR(ast->nodeData.instruction); break;
		case M_TYPE: encoding = encodeM(ast->nodeData.instruction, parser->symbolTable); break;
		case BU_TYPE: encoding = encodeBu(ast->nodeData.instruction); break;
		case BC_TYPE: emitWarning(WARN_UNIMPLEMENTED, NULL, "BC_TYPE instruction encoding not yet implemented."); break;
		case BI_TYPE: emitWarning(WARN_UNIMPLEMENTED, NULL, "BI_TYPE instruction encoding not yet implemented."); break;
		case S_TYPE: encoding = encodeS(ast->nodeData.instruction); break;
		default: break;
	}


	if (codegen->text.instructionCount == codegen->text.instructionCapacity) {
		codegen->text.instructionCapacity += 5;
		uint32_t* temp = (uint32_t*) realloc(codegen->text.instructions, codegen->text.instructionCapacity * sizeof(uint32_t));
		if (!temp) emitError(ERR_MEM, NULL, "Could not reallocate memory of instruction encodings.");
		log("New instruction array: %p", temp);
		codegen->text.instructions = temp;
	}
	log("Writing to index %d (address %p)", codegen->text.instructionCount, &codegen->text.instructions[codegen->text.instructionCount]);
	codegen->text.instructions[codegen->text.instructionCount] = encoding;
	codegen->text.instructionCount++;
}

static void gendata(Parser* parser, CodeGen* codegen, int dataIndex) {
	initScope("gendata");
	// TODO
/*
	data_entry_t* dataEntry = parser->dataTable.[dataIndex];
	if (!dataEntry) {
		emitError(ERR_INTERNAL, NULL, "Data entry at index %d is NULL.", dataIndex);
		return;
	}

	log("  Generating data entry %d of type %d at address 0x%08X with size %d bytes.", dataIndex, dataEntry->type, dataEntry->address, dataEntry->size);

	// Depending on the type of data entry, write to the appropriate section
	switch (dataEntry->type) {
		case BYTES_TYPE:
			// Write to data or const section depending on which section is active
			if (parser->sectionTable->activeSection == DATA_SECT_N) {
				// Data section
				for (int i = 0; i < dataEntry->size; i++) {
					if (codegen->data.dataCount == codegen->data.dataCapacity) {
						codegen->data.dataCapacity *= 2;
						uint8_t* temp = (uint8_t*) realloc(codegen->data.data, sizeof(uint8_t) * codegen->data.dataCapacity);
						if (!temp) emitError(ERR_MEM, NULL, "Failed to reallocate memory for data section.");
						codegen->data.data = temp;
					}
					// Each expression in the data entry is a byte
					Node* byteExpr = dataEntry->data[i];
					if (byteExpr->nodeType != ND_NUMBER) {
						emitError(ERR_INTERNAL, NULL, "Data entry expression is not a number node.");
						continue;
					}
					NumNode* numNode = byteExpr->nodeData.number;
					if (numNode->type != NTYPE_INT8 && numNode->type != NTYPE_INT16 && numNode->type != NTYPE_INT32) {
						emitError(ERR_INTERNAL, NULL, "Data entry number node is not an integer type.");
						continue;
					}
					int8_t byteValue = (int8_t) numNode->value.int32Value; // Just take the lowest byte
					codegen->data.data[codegen->data.dataCount++] = byteValue;
					log("    Wrote byte 0x%02X to data section.", (uint8_t) byteValue);
				}
			} else if (parser->sectionTable->activeSection == CONST_SECT_N) {
				// Const section
				for (int i = 0; i < dataEntry->size; i++) {
					if (codegen->consts.dataCount == codegen->consts.dataCapacity) {
						codegen->consts.dataCapacity *= 2;
						uint8_t* temp = (uint8_t*) realloc(codegen->consts.data, sizeof(uint8_t) * codegen->consts.dataCapacity);
						if (!temp) emitError(ERR_MEM, NULL, "Failed to reallocate memory for const section.");
						codegen->consts.data = temp;
					}
					// Each expression in the data entry is a byte
					Node* byteExpr = dataEntry->data[i];
					if (byteExpr->nodeType != ND_NUMBER) {
						emitError(ERR_INTERNAL, NULL, "Data entry expression is not a number node.");
						continue;
					}
					NumNode* numNode = byteExpr->nodeData.number;
					if (numNode->type != NTYPE_INT8 && numNode->type != NTYPE_INT16 && numNode->type != NTYPE_INT32) {
						emitError(ERR_INTERNAL, NULL, "Data entry number node is not an integer type.");
						continue;
					}
					int8_t byteValue = (int8_t) numNode->value.int32Value; // Just take the lowest byte
					codegen->consts.data[codegen->consts.dataCount++] = byteValue;
					log("    Wrote byte 0x%02X to const section.", (uint8_t) byteValue);
				}
			} else {
				emitError(ERR_INTERNAL, NULL, "Data entry in invalid section %d.", parser->sectionTable->activeSection);
			}
			break;
		case FLOATS_TYPE:
			// Write to data or const section depending on which section is active
			if (parser->sectionTable->activeSection == DATA_SECT_N) {
				// Data section
				for (int i = 0; i < dataEntry->size / 4; i++) {
					if (codegen->data.dataCount + 4 > codegen->data.dataCapacity) {
						codegen->data.dataCapacity *= 2;
						uint8_t* temp = (uint8_t*) realloc(codegen->data.data, sizeof(uint8_t) * codegen->data.dataCapacity);
						if (!temp) emitError(ERR_MEM, NULL, "Failed to reallocate memory for data section.");
						codegen->data.data = temp;
					}
					// Each expression in the data entry is a float
					Node* floatExpr = dataEntry->data[i];
					if (floatExpr->nodeType != ND_NUMBER) {
						emitError(ERR_INTERNAL, NULL, "Data entry expression is not a number node.");
						continue;
					}
					NumNode* numNode = floatExpr->nodeData.number;
					if (numNode->type != NTYPE_FLOAT) {
						emitError(ERR_INTERNAL, NULL, "Data entry number node is not a float type.");
						continue;
					}
					float floatValue = numNode->value.floatValue;
					// Write the float as 4 bytes in little-endian order
					uint8_t* floatBytes = (uint8_t*) &floatValue;
					for (int b = 0; b < 4; b++) {
						codegen->data.data[codegen->data.dataCount++] = floatBytes[b];
					}
					log("    Wrote float %f to data section.", floatValue);
				}
			} else if (parser->sectionTable->activeSection == CONST_SECT_N) {
				// Const section
				for (int i = 0; i < dataEntry->size / 4; i++) {
					if (codegen->consts.dataCount + 4 > codegen->consts.dataCapacity) {
						codegen->consts.dataCapacity *= 2;
						uint8_t* temp = (uint8_t*) realloc(codegen->consts.data, sizeof(uint8_t) * codegen->consts.dataCapacity);
						if (!temp) emitError(ERR_MEM, NULL, "Failed to reallocate memory for const section.");
						codegen->consts.data = temp;
					}
					// Each expression in the data entry is a float
					Node* floatExpr = dataEntry->data[i];
					if (floatExpr->nodeType != ND_NUMBER) {	
						emitError(ERR_INTERNAL, NULL, "Data entry expression is not a number node.");
						continue;
					}
					NumNode* numNode = floatExpr->nodeData.number;
					if (numNode->type != NTYPE_FLOAT) {
						emitError(ERR_INTERNAL, NULL, "Data entry number node is not a float type.");
						continue;
					}
					float floatValue = numNode->value.floatValue;
					// Write the float as 4 bytes in little-endian order
					uint8_t* floatBytes = (uint8_t*) &floatValue;
					for (int b = 0; b < 4; b++) {
						codegen->consts.data[codegen->consts.dataCount++] = floatBytes[b];
					}
					log("    Wrote float %f to const section.", floatValue);
				}
			} else {
				emitError(ERR_INTERNAL, NULL, "Data entry in invalid section %d.", parser->sectionTable->activeSection);
			}
			break;
		default:
			emitError(ERR_INTERNAL, NULL, "Data entry has unknown type %d.", dataEntry->type);
			break;
	}*/
}

void gencode(Parser* parser, CodeGen* codegen) {
	initScope("gencode");

	int j = 0; // Just a dummy variable for now, will be used to track data entries
	for (int i = 0; i < parser->astCount; i++) {
		Node* ast = parser->asts[i];
		log("Generating code for AST %d (%p):", i, ast);

		linedata_ctx linedata = {
			.linenum = ast->token->linenum,
			.source = ssGetString(ast->token->sstring)
		};

		// Each ast root will be either a label, instruction, or directive
		// Labels can be ignored

		switch (ast->nodeType) {
			case ND_INSTRUCTION:
				log("  Instruction");
				gentext(parser, codegen, ast);
				break;
			case ND_DIRECTIVE:
				// emitWarning(WARN_UNIMPLEMENTED, &linedata, "Directive codegen.");
				// break;

				log("  Directive");
				// Certain directives are useless
				// The directives to care about all the data ones
				if (ast->token->type < TK_D_STRING || ast->token->type > TK_D_ALIGN) {
					log("    Ignoring directive %s", ast->token->lexeme);
					break;
				}
				
				// Data directives are stored in the data table
				log("    Processing directive %s", ast->token->lexeme);
				emitWarning(WARN_UNIMPLEMENTED, &linedata, "Data directive codegen not yet implemented.");
				break;
				gendata(parser, codegen, j);
				j++;
				break;
			default:
				log("  AST root is neither instruction nor directive, ignoring.");
				break;
		}
	}
}


void displayCodeGen(CodeGen* codegen) {
	log("CodeGen State:");
	log("Text Section: %d instructions", codegen->text.instructionCount);
	for (int i = 0; i < codegen->text.instructionCount; i++) {
		log("  [%04d] 0x%08X", i, codegen->text.instructions[i]);
	}
	log("Data Section: %d bytes", codegen->data.dataCount);
	for (int i = 0; i < codegen->data.dataCount; i++) {
		if (i % 16 == 0) log("  [%04d] ", i);
		log("%02X ", codegen->data.data[i]);
		if (i % 16 == 15 || i == codegen->data.dataCount - 1) log("\n");
	}
	log("Const Section: %d bytes", codegen->consts.dataCount);
	for (int i = 0; i < codegen->consts.dataCount; i++) {
		if (i % 16 == 0) log("  [%04d] ", i);
		log("%02X ", codegen->consts.data[i]);
		if (i % 16 == 15 || i == codegen->consts.dataCount - 1) log("\n");
	}
}