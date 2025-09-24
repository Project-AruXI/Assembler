# Test Suite

This directory contains the Go-based test suites for the assembler project. Each subdirectory is a Go package that tests a specific component or aspect of the assembler. Most tests use the library version of the assembler components for direct and efficient testing.

## Test Packages

- **lexer/**: Tests the assembler's lexer component.
- **parser/**: Tests the parser component.
- **codegen/**: Tests the code generation logic.
- **e2e/**: End-to-end tests that exercise the full assembler pipeline from input to output.

> **Note:** Each package/directory contains a Go wrapper file (`wrapper.go`). This is required because Go's `testing` package cannot directly interact with C code via `cgo` without a Go entry point. The wrapper files expose C functions to Go for testing.

## Library Path Requirement

Many tests dynamically link against shared libraries built in the `out` directory. Before running tests, ensure the `out` directory is in your `LD_LIBRARY_PATH`:

```sh
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:[path-to-repo]/out"
```

## Running Tests

You can use the standard Go test tool to run tests at different levels:

- **Run all tests in a package/directory:**
	```sh
	go test -v
	```

- **Run all tests in a specific file (by function pattern):**
	```sh
	go test -run [Test]?[Function] -v
	```
	Replace `[Function]` with the test function to want to run (e.g., `CommentsAndWhitespace` for `TestCommentsAndWhitespace()` in `commentsWhitespace_test.go`). It is optional to include "Test".

- **Run a single test in a file:**
	```sh
	go test -run [Test]?[Function]/[num] -v
	```
	Replace `[num]` with the exact number of the test (e.g., `TestCommentsAndWhitepsace\0`).

## Example

To run all tests in the `lexer` package:

```sh
cd lexer
go test -v
```

To run only the `TestLexerWhitespace` test:

```sh
go test -run TestLexerWhitespace -v
```
