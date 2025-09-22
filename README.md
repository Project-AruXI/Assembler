# Aru Assembler

Aru Assembler is a modular assembler project designed for flexibility and clarity. The project is organized to separate core components, data structures, headers, sample assembly files, and a Go-based test suite.

## Directory Structure

```
Assembler/
├── Makefile
├── README.md
├── assembler.c           # Main entry point
├── components/           # Core moving parts (lexer, parser, codegen, diagnostics, ...)
├── headers/              # All header files
├── samples/              # Example assembly source files
├── structures/           # Data structures (symbol table, AST, relocation table, ...)
└── testsuite/            # Test suite for each component (written in Go)
```

## Folder Descriptions

- **components/**: Contains the main logic for the assembler, such as the lexer, parser, code generator, and diagnostics modules.
- **headers/**: All C header files for shared types, function declarations, and interfaces.
- **samples/**: Example assembly files for testing and demonstration.
- **structures/**: Implementation of data structures like the symbol table, AST, and relocation table.
- **testsuite/**: Test suite for each component, implemented in Go for robust and modern testing.

## Building

Use the provided `Makefile` to build the assembler:

```sh
make
```

## Testing

Navigate to the `testsuite/` directory and run the Go tests:

```sh
cd testsuite
go test ./...
```

## License

See [LICENSE](LICENSE) for details.
