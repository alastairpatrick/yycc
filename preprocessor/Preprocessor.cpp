#include "Preprocessor.h"

#include "Message.h"
#include "Identifier.h"

string unescape_string(string_view text, const Location& location);

TokenKind Preprocessor::next_token() {
    for (;;) {
        next_token_internal();
        switch (token) {
            default: {
              return token;
          } case TOK_IDENTIFIER: {
              token = id_lexer.next_token(source.token_input());
              return id_lexer.size() == source.text().size() ? token : TOK_IDENTIFIER;
          } case TOK_PP_NUMBER: {
              token = num_lexer.next_token(source.token_input());
              return num_lexer.size() == source.text().size() ? token : TOK_PP_NUMBER;
          } case '\n': {
              continue;
          } case '#': {
              next_token_internal();
              handle_directive();
              continue;
          } case TOK_PP_UNRECOGNIZED: {
              message(Severity::ERROR, location()) << "unexpected character '" << text() << "'\n";
              continue;
          } case TOK_PP_UNTERMINATED_COMMENT: {
              message(Severity::ERROR, location()) << "unterminated comment\n";
              continue;
          }
        }
    }
}

void Preprocessor::handle_directive() {
    switch (token) {
        case TOK_PP_LINE: {
          handle_line_directive();
          break;
      }
    }

    require_eol();
}

void Preprocessor::skip_to_eol() {
    while (token && token != '\n') {
        next_token_internal();
    }
}

void Preprocessor::require_eol() {
    if (token != '\n') {
        message(Severity::ERROR, location()) << "unexpected token in directive\n";
    }

    skip_to_eol();
}

TokenKind Preprocessor::next_token_internal() {
    return token = source.next_token();
}

Identifier Preprocessor::identifier() const
{
    return Identifier(text());
}

void Preprocessor::handle_line_directive() {
    next_token_internal();

    if (token != TOK_PP_NUMBER) return;

    size_t line;
    auto result = from_chars(text().data(), text().data() + text().size(), line, 10);
    if (result.ec != errc{} || line <= 0 || result.ptr != text().data() + text().size()) return;

    next_token_internal();

    string filename;
    if (token == TOK_STRING_LITERAL) {
        filename = unescape_string(text(), location());
        next_token_internal();
    }

    if (token != '\n') return;

    source.set_lineno(line - 1);
    if (!filename.empty()) {
        source.set_filename(move(filename));
    }
}
