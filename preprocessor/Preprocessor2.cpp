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
    auto begin = fragment().position;
    auto end = begin;
    while (token && token != '\n') {
        end = fragment().position + fragment().length;
        next_token_internal();
    }

    stream << Fragment(begin, end - begin).text() << '\n';
}

void Preprocessor2::handle_pragma_directive() {
    skip_to_eol();
}
