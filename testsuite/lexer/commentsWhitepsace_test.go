package lexerTests

import "testing"

func TestCommentsAndWhitespace(t *testing.T) {
	lexer := lexerInitLexer()
	defer lexerDeinitLexer(lexer)

	expectedTokenCount := 0

	// Test case 0: Line with only a comment
	t.Run("0", func(t *testing.T) {
		line := "  % This is a comment"

		t.Logf("%sInput string is `%s`%s", YELLOW, line, RESET)

		lexerLexLine(lexer, line)

		if int(lexer.tokenCount) != expectedTokenCount {
			t.Errorf("%sExpected 0 tokens, got %d%s", RED, int(lexer.tokenCount), RESET)
		} else {
			t.Logf("%sPassed!%s", GREEN, RESET)
		}
	})

	t.Run("1", func(t *testing.T) {
		line := "add x1, x2, #1   % inline comment"

		t.Logf("%sInput string is `%s`%s", YELLOW, line, RESET)

		lexerLexLine(lexer, line)
		expectedTokenCount += 6

		if int(lexer.tokenCount) != expectedTokenCount {
			t.Errorf("%sExpected 6 tokens, got %d%s", RED, lexer.tokenCount, RESET)
		} else {
			t.Logf("%sPassed!%s", GREEN, RESET)
		}
	})

	t.Run("2", func(t *testing.T) {
		lines := []string {
			"% comment 1",
			"   % comment 2",
			"\t% comment 3",
		}

		for i, line := range lines {
			t.Logf("%sInput string is `%s`%s", YELLOW, line, RESET)
			lexerLexLine(lexer, line)
			if int(lexer.tokenCount) != expectedTokenCount {
				t.Errorf("%sLine %d: Expected 6 tokens, got %d%s", RED, i, lexer.tokenCount, RESET)
			} else {
				t.Logf("%sLine %d: Passed!%s", GREEN, i, RESET)
			}
		}
	})
}
