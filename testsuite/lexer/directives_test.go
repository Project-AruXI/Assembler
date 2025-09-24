package lexerTests

import "testing"


func TestDefs(t *testing.T) {
	tokenTypeInit()
	lexer := lexerInitLexer()
	defer lexerDeinitLexer(lexer)

	expectedTokenCount := 0

	t.Run("0", func(t *testing.T) {
		line := ".def Node{ value:8.next::Node.}"

		t.Logf("%sInput string is `%s`%s", YELLOW, line, RESET)

		lexerLexLine(lexer, line)

		expectedTokenCount = 12

		if int(lexer.tokenCount) != expectedTokenCount {
			t.Errorf("%sExpected 12 tokens, got %d%s", RED, int(lexer.tokenCount), RESET)
			printLexerTokens(lexer)
		} else {
			expectedTokens := []struct {
				_type int
				lexeme string
			}{
				{TK_DIRECTIVE, ".def"},
				{TK_IDENTIFIER, "Node"},
				{TK_LBRACKET, "{"},
				{TK_IDENTIFIER, "value"},
				{TK_COLON, ":"},
				{TK_INTEGER, "8"},
				{TK_DOT, "."},
				{TK_IDENTIFIER, "next"},
				{TK_COLON_COLON, "::"},
				{TK_IDENTIFIER, "Node"},
				{TK_DOT, "."},
				{TK_RBRACKET, "}"},
			}

			passed := true
			for i, expected := range expectedTokens {
				token := lexerGetToken(lexer, i)
				if !assertToken(token, _Ctype_tokenType(expected._type), expected.lexeme) {
					passed = false
					t.Errorf("%sToken %d mismatch: expected (%s, `%s`), got (%s, `%s`)%s",
						RED, i, tokenTypeToString(_Ctype_tokenType(expected._type)), expected.lexeme, tokenTypeToString(token._type), fromCharPtrToGoString(token.lexeme), RESET)
				}
			}

			if (passed) {
				t.Logf("%sPassed!%s", GREEN, RESET)
			}
		}
	})

	t.Run("1", func(t *testing.T) {
		lexerResetLexer(lexer)

		lines := []string{
			".def Node {",
			"value :8 .next :: Node .",
			"}",
		}
		for _, line := range lines {
			t.Logf("%sInput string is `%s`%s", YELLOW, line, RESET)
			lexerLexLine(lexer, line)
		}

		expectedTokenCount = 12

		if int(lexer.tokenCount) != expectedTokenCount {
			t.Errorf("%sExpected 12 tokens, got %d%s", RED, int(lexer.tokenCount), RESET)
			printLexerTokens(lexer)
		} else {
			expectedTokens := []struct {
				_type int
				lexeme string
			}{
				{TK_DIRECTIVE, ".def"},
				{TK_IDENTIFIER, "Node"},
				{TK_LBRACKET, "{"},
				{TK_IDENTIFIER, "value"},
				{TK_COLON, ":"},
				{TK_INTEGER, "8"},
				{TK_DOT, "."},
				{TK_IDENTIFIER, "next"},
				{TK_COLON_COLON, "::"},
				{TK_IDENTIFIER, "Node"},
				{TK_DOT, "."},
				{TK_RBRACKET, "}"},
			}

			passed := true
			for i, expected := range expectedTokens {
				token := lexerGetToken(lexer, i)
				if !assertToken(token, _Ctype_tokenType(expected._type), expected.lexeme) {
					passed = false
					t.Errorf("%sToken %d mismatch: expected (%s, `%s`), got (%s, `%s`)%s",
						RED, i, tokenTypeToString(_Ctype_tokenType(expected._type)), expected.lexeme, tokenTypeToString(token._type), fromCharPtrToGoString(token.lexeme), RESET)
				}
			}
			
			if (passed) {
				t.Logf("%sPassed!%s", GREEN, RESET)
			}
		}
	})

	t.Run("2", func(t *testing.T) {
		lexerResetLexer(lexer)

		lines := []string{
			"   .def   Node  {   % This is a comment",
			"value :8 .next :: Node .  % Another comment",
			"}   ",
		}
		for _, line := range lines {
			t.Logf("%sInput string is `%s`%s", YELLOW, line, RESET)
			lexerLexLine(lexer, line)
		}
		expectedTokenCount = 12

		if int(lexer.tokenCount) != expectedTokenCount {
			t.Errorf("%sExpected 12 tokens, got %d%s", RED, int(lexer.tokenCount), RESET)
			printLexerTokens(lexer)
		} else {
			expectedTokens := []struct {
				_type int
				lexeme string
			}{
				{TK_DIRECTIVE, ".def"},
				{TK_IDENTIFIER, "Node"},
				{TK_LBRACKET, "{"},
				{TK_IDENTIFIER, "value"},
				{TK_COLON, ":"},
				{TK_INTEGER, "8"},
				{TK_DOT, "."},
				{TK_IDENTIFIER, "next"},
				{TK_COLON_COLON, "::"},
				{TK_IDENTIFIER, "Node"},
				{TK_DOT, "."},
				{TK_RBRACKET, "}"},
			}

			passed := true
			for i, expected := range expectedTokens {
				token := lexerGetToken(lexer, i)
				if !assertToken(token, _Ctype_tokenType(expected._type), expected.lexeme) {
					passed = false
					t.Errorf("%sToken %d mismatch: expected (%s, `%s`), got (%s, `%s`)%s",
						RED, i, tokenTypeToString(_Ctype_tokenType(expected._type)), expected.lexeme, tokenTypeToString(token._type), fromCharPtrToGoString(token.lexeme), RESET)
				}
			}
			
			if (passed) {
				t.Logf("%sPassed!%s", GREEN, RESET)
			}
		}
	})

	// Test with ".def MyDef_Name\n{field1:32.field_2::Other. \n}"
	t.Run("3", func(t *testing.T) {
		lexerResetLexer(lexer)

		lines := []string{
			".def MyDef_Name",
			"{field1:32.field_2::Other.",
			"}",
		}
		for _, line := range lines {
			t.Logf("%sInput string is `%s`%s", YELLOW, line, RESET)
			lexerLexLine(lexer, line)
		}
		expectedTokenCount = 12

		if int(lexer.tokenCount) != expectedTokenCount {
			t.Errorf("%sExpected 12 tokens, got %d%s", RED, int(lexer.tokenCount), RESET)
			printLexerTokens(lexer)
		} else {
			expectedTokens := []struct {
				_type int
				lexeme string
			}{
				{TK_DIRECTIVE, ".def"},
				{TK_IDENTIFIER, "MyDef_Name"},
				{TK_LBRACKET, "{"},
				{TK_IDENTIFIER, "field1"},
				{TK_COLON, ":"},
				{TK_INTEGER, "32"},
				{TK_DOT, "."},
				{TK_IDENTIFIER, "field_2"},
				{TK_COLON_COLON, "::"},
				{TK_IDENTIFIER, "Other"},
				{TK_DOT, "."},
				{TK_RBRACKET, "}"},
			}

			passed := true
			for i, expected := range expectedTokens {
				token := lexerGetToken(lexer, i)
				if !assertToken(token, _Ctype_tokenType(expected._type), expected.lexeme) {
					passed = false
					t.Errorf("%sToken %d mismatch: expected (%s, `%s`), got (%s, `%s`)%s",
						RED, i, tokenTypeToString(_Ctype_tokenType(expected._type)), expected.lexeme, tokenTypeToString(token._type), fromCharPtrToGoString(token.lexeme), RESET)
				}
			}
			
			if (passed) {
				t.Logf("%sPassed!%s", GREEN, RESET)
			}
		}
	})
}

func TestDefsIncorrect(t *testing.T) {}


func TestType(t *testing.T) {
	tokenTypeInit()
	lexer := lexerInitLexer()
	defer lexerDeinitLexer(lexer)

	expectedTokenCount := 0

	t.Run("0", func(t *testing.T) {
		line := ".type ARR0, $object"

		t.Logf("%sInput string is `%s`%s", YELLOW, line, RESET)

		lexerLexLine(lexer, line)

		expectedTokenCount = 4

		if int(lexer.tokenCount) != expectedTokenCount {
			t.Errorf("%sExpected 4 tokens, got %d%s", RED, int(lexer.tokenCount), RESET)
			printLexerTokens(lexer)
		} else {
			expectedTokens := []struct {
				_type int
				lexeme string
			}{
				{TK_DIRECTIVE, ".type"},
				{TK_IDENTIFIER, "ARR0"},
				{TK_COMMA, ","},
				{TK_MAIN_TYPE, "$object"},
			}

			for i, expected := range expectedTokens {
				token := lexerGetToken(lexer, i)
				if !assertToken(token, _Ctype_tokenType(expected._type), expected.lexeme) {
					t.Errorf("%sToken %d mismatch: expected (%s, `%s`), got (%s, `%s`)%s",
						RED, i, tokenTypeToString(_Ctype_tokenType(expected._type)), expected.lexeme, tokenTypeToString(token._type), fromCharPtrToGoString(token.lexeme), RESET)
				}
			}

			t.Logf("%sPassed!%s", GREEN, RESET)
		}
	})

	// .type ARR1,$obj.array
	t.Run("1", func(t *testing.T) {
		lexerResetLexer(lexer)

		line := ".type ARR1,$obj.array"

		t.Logf("%sInput string is `%s`%s", YELLOW, line, RESET)

		lexerLexLine(lexer, line)

		expectedTokenCount = 6

		if int(lexer.tokenCount) != expectedTokenCount {
			t.Errorf("%sExpected 6 tokens, got %d%s", RED, int(lexer.tokenCount), RESET)
			printLexerTokens(lexer)
		} else {
			expectedTokens := []struct {
				_type int
				lexeme string
			}{
				{TK_DIRECTIVE, ".type"},
				{TK_IDENTIFIER, "ARR1"},
				{TK_COMMA, ","},
				{TK_MAIN_TYPE, "$obj"},
				{TK_DOT, "."},
				{TK_IDENTIFIER, "array"},
			}

			for i, expected := range expectedTokens {
				token := lexerGetToken(lexer, i)
				if !assertToken(token, _Ctype_tokenType(expected._type), expected.lexeme) {
					t.Errorf("%sToken %d mismatch: expected (%s, `%s`), got (%s, `%s`)%s",
						RED, i, tokenTypeToString(_Ctype_tokenType(expected._type)), expected.lexeme, tokenTypeToString(token._type), fromCharPtrToGoString(token.lexeme), RESET)
				}
			}

			t.Logf("%sPassed!%s", GREEN, RESET)
		}
	})

	// .type ARR2, $obj.array.word
	t.Run("2", func(t *testing.T) {
		lexerResetLexer(lexer)

		line := ".type ARR2, $obj.array.word"

		t.Logf("%sInput string is `%s`%s", YELLOW, line, RESET)

		lexerLexLine(lexer, line)

		expectedTokenCount = 8

		if int(lexer.tokenCount) != expectedTokenCount {
			t.Errorf("%sExpected 8 tokens, got %d%s", RED, int(lexer.tokenCount), RESET)
			printLexerTokens(lexer)
		} else {
			expectedTokens := []struct {
				_type int
				lexeme string
			}{
				{TK_DIRECTIVE, ".type"},
				{TK_IDENTIFIER, "ARR2"},
				{TK_COMMA, ","},
				{TK_MAIN_TYPE, "$obj"},
				{TK_DOT, "."},
				{TK_IDENTIFIER, "array"},
				{TK_DOT, "."},
				{TK_IDENTIFIER, "word"},
			}

			for i, expected := range expectedTokens {
				token := lexerGetToken(lexer, i)
				if !assertToken(token, _Ctype_tokenType(expected._type), expected.lexeme) {
					t.Errorf("%sToken %d mismatch: expected (%s, `%s`), got (%s, `%s`)%s",
						RED, i, tokenTypeToString(_Ctype_tokenType(expected._type)), expected.lexeme, tokenTypeToString(token._type), fromCharPtrToGoString(token.lexeme), RESET)
				}
			}

			t.Logf("%sPassed!%s", GREEN, RESET)
		}
	})


}

func TestTypeIncorrect(t *testing.T) {}




