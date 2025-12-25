#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "argparse.h"
#include "diagnostics.h"
#include "sds.h"
#include "lexer.h"
#include "config.h"
#include "parser.h"
#include "codegen.h"
#ifdef _WIN32
#include "getline.h"
#endif

Config config;

char* parseArgs(int argc, char const* argv[]) {
	// Init config with defaults
	config.useDebugSymbols = false;
	config.warningAsFatal = false;
	config.outbin = "out.ao";
	config.warnings = WARN_FLAG_ALL; // Enable all warnings by default
	config.enhancedFeatures = FEATURE_NONE; // Disable all enhanced features by default

	bool warningAsFatal = false;
	bool showVersion = false;
	char* infile = NULL;

	struct argparse_option options[] = {
		OPT_STRING('o', NULL, &config.outbin, "output filename", NULL, 0, 0),
		OPT_BOOLEAN('g', NULL, &config.useDebugSymbols, "enable debug info", NULL, 0, 0),
		OPT_BOOLEAN('W', "no-warn", &config.warnings, "disable warnings", NULL, 0, 0),
		OPT_BOOLEAN('F', "fatal-warning", &warningAsFatal, "treat warnings as errors", NULL, 0, 0),
		OPT_BOOLEAN('v', "version", &showVersion, "show version and exit", NULL, 0, 0),
		OPT_BIT('t', "enable-types", &config.enhancedFeatures, "enable enhanced typing features", NULL, FEATURE_TYPES, 0),
		OPT_BIT('m', "enable-macros", &config.enhancedFeatures, "enable macros feature", NULL, FEATURE_MACROS, 0),
		OPT_BIT('p', "enable-ptr-deref", &config.enhancedFeatures, "enable pointer dereferencing in expressions", NULL, FEATURE_PTR_DEREF, 0),
		OPT_BIT('f', "enable-field-access", &config.enhancedFeatures, "enable struct/array field access in expressions", NULL, FEATURE_FIELD_ACCESS, 0),
		OPT_HELP(),
		OPT_END(),
	};

	const char* const usages[] = {
		"arxsm [options] file",
		NULL
	};

	struct argparse argparse;
	argparse_init(&argparse, options, usages, 0);
	argparse_describe(&argparse, "Aru Assembler", NULL);
	int nparsed = argparse_parse(&argparse, argc, argv);

	if (showVersion) {
		printf("Aru Assembler version 1.0.0\n");
		exit(0);
	}


	// Remaining arguments after options are input files
	if (argc - nparsed < 1) {
		fprintf(stderr, "No input file specified.\n");
		argparse_usage(&argparse);
		exit(-1);
	}

	log("Output file: %s", config.outbin);

	const char* allowedExts[] = { ".s", ".as", ".ars", ".adecl" };

	for (int i = 0; i < nparsed; i++) {
		const char* dot = strrchr(argv[i], '.');
		if (!dot || dot == argv[i] || *(dot-1) == '/') continue;

		for (int j = 0; i < 1; j++) {
			if (strcmp(dot, allowedExts[j]) != 0) continue;

			infile = (char*) argv[i];
			break;
		}
	}

	if (!infile) emitError(ERR_INTERNAL, NULL, "Input file is not a valid assembly file.");

	return infile;
}

int main(int argc, char const* argv[]) {
	initScope("main");

	char* infile = parseArgs(argc, argv);

	FILE* source = fopen(infile, "r");
	if (!source) emitError(ERR_IO, NULL, "Failed to open input file: %s", infile);

	Lexer* lexer = initLexer();

	char* line = NULL;
	size_t n;

	ssize_t read = getline(&line, &n, source);
	while (read != -1) {
		lexLine(lexer, line);

		read = getline(&line, &n, source);
	}
	free(line);
	fclose(source);

	rlog("\nLexed %d lines. Read %d tokens:", lexer->linenum, lexer->tokenCount);
	// Show contents of lexer's tokens
	for (int i = 0; i < lexer->tokenCount; i++) {
		printToken(lexer->tokens[i]);
	}
	rlog("\n");

	// Finished lexing, now parse

	// First initialize the tables
	SymbolTable* symbolTable = initSymbolTable();
	SectionTable* sectionTable = initSectionTable();
	StructTable* structTable = initStructTable();
	DataTable* dataTable = initDataTable();
	RelocTable* relocTable = initRelocTable();

	ParserConfig pconfig = {
		.warningAsFatal = config.warningAsFatal,
		.warnings = config.warnings,
		.enhancedFeatures = config.enhancedFeatures
	};
	Parser* parser = initParser(lexer->tokens, lexer->tokenCount, pconfig);
	setTables(parser, sectionTable, symbolTable, structTable, dataTable, relocTable);


	parse(parser);

	rlog("\n\n");
	rlog("Parsed %d ASTs:", parser->astCount);
	for (int i = 0; i < parser->astCount; i++) {
		rlog("AST %d:", i);
		printAST(parser->asts[i]);
	}
	rlog("\n");

	CodeGen* codegen = initCodeGenerator(sectionTable, symbolTable, relocTable);
	gencode(parser, codegen);
	// writeBinary(codegen, config.outbin);
	
	displaySymbolTable(symbolTable);
	displaySectionTable(sectionTable);
	displayStructTable(structTable);
	displayDataTable(dataTable);
	displayCodeGen(codegen);
	displayRelocTable(relocTable);

	deinitLexer(lexer);
	deinitParser(parser);
	deinitStructTable(structTable);
	deinitSectionTable(sectionTable);
	deinitSymbolTable(symbolTable);
	deinitRelocTable(relocTable);
	deinitCodeGenerator(codegen);

	return 0;
}
