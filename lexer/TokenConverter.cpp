#include "TokenConverter.h"

#include "Message.h"
#include "Identifier.h"

string unescape_string(string_view text, const Location& location);

int TokenConverter::next_token() {
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

void TokenConverter::handle_directive() {
    next_token_internal();
    switch (token) {
        default: {
          // All other directives should have been handled by the preprocessor.
          assert(false);
          break;
      } case TOK_PP_ERROR: {
          handle_error_directive();
          break;
      } case TOK_PP_LINE: {
          handle_line_directive();
          break;
      } case TOK_PP_PRAGMA: {
          skip_to_eol();
          break;
      } case TOK_PP_UNRECOGNIZED: {
          break;
      }
    }

    if (token != '\n') {
        message(Severity::ERROR, location()) << "unexpected token in directive\n";
    }

    skip_to_eol();
}

void TokenConverter::handle_error_directive() {
    auto& stream = message(Severity::ERROR, location());
    next_token_internal();
    auto begin = source.text().data();
    auto end = begin;
    while (token && token != '\n') {
        end = source.text().data() + source.text().size();
        next_token_internal();
    }

    stream << string_view(begin, end - begin) << '\n';
}

void TokenConverter::handle_line_directive() {
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

void TokenConverter::handle_pragma_directive() {
    skip_to_eol();
}

void TokenConverter::skip_to_eol() {
    while (token && token != '\n') {
        next_token_internal();
    }
}

TokenKind TokenConverter::next_token_internal() {
    return token = source.next_token();
}

Identifier TokenConverter::identifier() const
{
    return Identifier(text());
}