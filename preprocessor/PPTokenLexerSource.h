#ifndef PP_TOKEN_LEXER_STREAM_H
#define PP_TOKEN_LEXER_STREAM_H

#include "Location.h"
#include "PPTokenLexer.yy.h"
#include "Token.h"

struct PPTokenLexerSource {
  explicit PPTokenLexerSource(const Input& input);
  void operator=(const PPTokenLexerSource&) = delete;

  TokenKind next_token() {
      return TokenKind(lexer.next_token());
  }

  string_view text() const {
      return string_view(lexer.matcher().begin(), lexer.size());
  }

  Input token_input() const {
      return Input(lexer.matcher().begin(), lexer.size());
  }

  Location location() const {
      return Location { lexer.lineno(), lexer.columno() + 1, current_filename };
  }

  void set_lineno(size_t line) {
      lexer.lineno(line);
  }

  void set_filename(string&& filename);

  size_t byte_offset() const {
    return lexer.matcher().first();
  }

private:
  PPTokenLexer lexer;

  unordered_set<string> filenames;
  string_view current_filename;
};

#endif

