#include "adecl.h"
#include "diagnostics.h"
#include "lexer.h"


FILE* openADECLFile(sds filename) {
	// TODO: Need to add a system of include paths, etc
	// For now, the file must be in the same directory where the assembler is ran
	FILE* file = fopen(filename, "r");
	return file;
}

static void validateASTS(Node** asts, int astCount) {
	initScope("validateASTS()");

	for (int i = 0; i < astCount; i++) {
		Node* root = asts[i];
		linedata_ctx linedata = {
			.linenum = root->token->linenum,
			.source = ssGetString(root->token->sstring)
		};
		if (root->nodeType != ND_DIRECTIVE) emitError(ERR_NOT_ALLOWED, &linedata, "Only directives are allowed in ADECL files.");

		// This is where the big assumption and trust that the parser updates the token's type to the actual directive
		// That will be used to determine what directive it is
		if (root->token->type != TK_D_SET && root->token->type != TK_D_EXTERN &&
				root->token->type != TK_D_TYPE && root->token->type != TK_D_SIZEOF &&
				root->token->type != TK_D_DEF && root->token->type != TK_D_INCLUDE) {
			emitError(ERR_INVALID_DIRECTIVE, &linedata, "Directive `%s` is not allowed in ADECL files.", root->token->lexeme);
		}
	}
}


void lexParseADECLFile(FILE* file, ADECL_ctx* context) {
	initScope("lexParseADECLFile()");

	Lexer* lexer = initLexer();

	char* line = NULL;
	size_t n;

	ssize_t read = getline(&line, &n, file);
	while (read != -1) {
		lexLine(lexer, line);

		read = getline(&line, &n, file);
	}
	free(line);
	fclose(file);

	// Now, since this reuses all of the logic as the main assembler,
	// The lexer and parser allows instructions and directives that are not valid in ADECL files
	// This can be fixed by either writing a new lexer and parser just for ADECL files,
	// Or by adding a mode to the existing lexer and parser to restrict what is allowed
	// Or just scan after the parser to check if all ASTs are valid
	// This might be an issue since errors are not caught early, and the need to loop through everything
	// Scanning after the parser is what will be done, for now

	log("\nLexed %d lines. Read %d tokens:", lexer->linenum, lexer->tokenCount);
	// Show contents of lexer's tokens
	for (int i = 0; i < lexer->tokenCount; i++) {
		printToken(lexer->tokens[i]);
	}
	log("\n");

	// Finished lexing, now parse

	// First initialize the tables
	SymbolTable* symbolTable = initSymbolTable();
	StructTable* structTable = initStructTable();

	ParserConfig pconfig = {
		.warningAsFatal = context->parentParserConfig.warningAsFatal,
		.warnings = context->parentParserConfig.warnings,
		.enhancedFeatures = context->parentParserConfig.enhancedFeatures
	};
	Parser* parser = initParser(lexer->tokens, lexer->tokenCount, pconfig);
	setTables(parser, NULL, symbolTable, structTable, NULL);

	parse(parser);

	validateASTS(parser->asts, parser->astCount);

	context->asts = parser->asts;
	context->astCount = parser->astCount;
	context->astCapacity = parser->astCapacity;
	context->symbolTable = symbolTable;
	context->structTable = structTable;

	free(lexer);
	free(parser);
}