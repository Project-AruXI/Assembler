package lexerTests

import "testing"

func TestCommentsAndWhitespace(t *testing.T) {
	lexer := lexerInitLexer()
	defer lexerDeinitLexer(lexer)

	// Test case 0: Line with only a comment
	t.Run("0", func(t *testing.T) {
		line := "  % This is a comment"
		
		t.Logf("%sInput string is `%s`%s", YELLOW, line, RESET)

		lexerLexLine(lexer, line)

		if lexer.tokenCount != 0 {
			t.Errorf("%sExpected 0 tokens, got %d%s", RED, lexer.tokenCount, RESET)
		} else {
			t.Logf("%sPassed!%s", GREEN, RESET)
		}
	})
}