#include "Preprocessor2.h"

#include "Message.h"

void Preprocessor2::handle_directive() {
    switch (token) {
        default: {
          Preprocessor::handle_directive();
          return;
      } case TOK_PP_ERROR: {
          handle_error_directive();
          break;
      } case TOK_PP_PRAGMA: {
          handle_pragma_directive(); 
          break;
      }
    }

    require_eol();
}

void Preprocessor2::handle_error_directive() {
    auto& stream = message(Severity::ERROR, location());
    next_token_internal();
    auto begin = lexer.text().data();
    auto end = begin;
    while (token && token != '\n') {
        auto text = lexer.text();
        end = text.data() + text.length();
        next_token_internal();
    }

    stream << string_view(begin, end - begin) << '\n';
}

void Preprocessor2::handle_pragma_directive() {
    skip_to_eol();
}
