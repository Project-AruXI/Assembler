#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "argparse.h"
#include "diagnostics.h"
#include "sds.h"
#include "lexer.h"

extern bool doWarn;

int main(int argc, char const* argv[]) {
	initScope("main");

	doWarn = true;
	bool useDebugSymbols = 0;
	bool warningAsFatal = 0;
	bool showVersion = 0;
	const char* outbin = "out.ao";
	char* infile = NULL;

	struct argparse_option options[] = {
		OPT_STRING('o', NULL, &outbin, "output filename", NULL, 0, 0),
		OPT_BOOLEAN('g', NULL, &useDebugSymbols, "enable debug info", NULL, 0, 0),
		OPT_BOOLEAN('W', "no-warn", &doWarn, "disable warnings", NULL, 0, 0),
		OPT_BOOLEAN('F', "fatal-warning", &warningAsFatal, "treat warnings as errors", NULL, 0, 0),
		OPT_BOOLEAN('v', "version", &showVersion, "show version and exit", NULL, 0, 0),
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
		return 0;
	}

	log("nparsed: %d", nparsed);
	log("argc: %d", argc);



	// Remaining arguments after options are input files
	if (argc - nparsed < 1) {
		fprintf(stderr, "No input file specified.\n");
		argparse_usage(&argparse);
		return 1;
	}

	log("Output file: %s\n", outbin);

	const char* allowedExts[] = { ".s", ".as", ".ars" };

	for (int i = 0; i < nparsed; i++) {
		log("Input file: %s\n", argv[i]);
		
		const char* dot = strrchr(argv[i], '.');
		if (!dot || dot == argv[i] || *(dot-1) == '/') continue;

		for (int j = 0; i < 1; j++) {
			if (strcmp(dot, allowedExts[j]) != 0) continue;

			infile = (char*) argv[i];
			break;
		}
	}

	if (!infile) emitError(ERR_INTERNAL, NULL, "Input file is not a valid assembly file.");

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


	deinitLexer(lexer);
	fclose(source);

	return 0;
}