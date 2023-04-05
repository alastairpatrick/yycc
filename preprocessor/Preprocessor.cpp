#include "Preprocessor.h"

#include "FileCache.h"
#include "lexer/Identifier.h"
#include "lexer/Unescape.h"
#include "Message.h"

Preprocessor::Preprocessor(bool preparse): preparse(preparse), text_stream(string_stream) {
}

Preprocessor::Preprocessor(string_view input, bool preparse): Preprocessor(preparse) {
    lexer.buffer(input);
}

void Preprocessor::in(const Input& input) {
    lexer.in(input);
}

TokenKind Preprocessor::next_token() {
    for (;;) {
        next_pp_token();
        switch (token) {
            default: {
              return commit_token(token, text());
          } case TOK_IDENTIFIER: {
              id_lexer.buffer(text());
              token = id_lexer.next_token();
              return commit_token(id_lexer.size() == lexer.fragment().length ? token : TOK_IDENTIFIER, text());
          } case TOK_PP_NUMBER: {
              num_lexer.buffer(text());
              token = num_lexer.next_token();
              return commit_token(num_lexer.size() == lexer.fragment().length ? token : TOK_PP_NUMBER, text());
          } case '\n': {
              continue;
          } case '#': {
              next_pp_token();
              if (!handle_directive()) return token;
              continue;
          } case TOK_PP_UNRECOGNIZED: {
              message(Severity::ERROR, location()) << "unexpected character '" << lexer.text() << "'\n";
              continue;
          } case TOK_PP_UNTERMINATED_COMMENT: {
              message(Severity::ERROR, location()) << "unterminated comment\n";
              continue;
          }
        }
    }
}

TokenKind Preprocessor::next_pp_token() {
    for (;;) {
        token = lexer.next_token();
        if (token != TOK_EOF || include_stack.empty()) break;

        lexer.pop_matcher();

        auto location = include_stack.back();
        include_stack.pop_back();

        lexer.lineno(location.line);
        lexer.set_filename(location.filename);
    }
    return token;
}

bool Preprocessor::handle_directive() {
    switch (token) {
        case TOK_PP_INCLUDE: {
          handle_include_directive();
          return true;
      } case TOK_PP_LINE: {
          handle_line_directive();
          break;
      }
    }

    if (!preparse) {
      switch (token) {
          case TOK_PP_ERROR: {
            handle_error_directive();
            break;
        } case TOK_PP_PRAGMA: {
            handle_pragma_directive(); 
            break;
        } case TOK_PP_TYPE: {
            return false;
        }
      }
    }

    require_eol();
    return true;
}

void Preprocessor::skip_to_eol() {
    while (token && token != '\n') {
        next_pp_token();
    }
}

void Preprocessor::require_eol() {
    if (token != '\n') {
        unexpected_directive_token();
    }

    skip_to_eol();
}

void Preprocessor::unexpected_directive_token() {
    message(Severity::ERROR, location()) << "unexpected token in directive\n";
}

TokenKind Preprocessor::commit_token(TokenKind token, string_view text) {
    if (preparse) {
        text_stream.locate(lexer.location());
        auto begin = string_stream.pcount();
        text_stream.write(text);
        auto end = string_stream.pcount();
        fragment = Fragment(begin, end - begin);
    } else {
        fragment = lexer.fragment();
    }

    return token;
}

bool Preprocessor::mark_root() const {
    return include_stack.empty();
}

string_view Preprocessor::output() {
    return string_view(string_stream.str(), string_stream.pcount());
}

void Preprocessor::handle_line_directive() {
    next_pp_token();

    if (token != TOK_PP_NUMBER) return;

    size_t line;
    auto text = lexer.text();
    auto result = from_chars(text.data(), text.data() + text.size(), line, 10);
    if (result.ec != errc{} || line <= 0 || result.ptr != text.data() + text.size()) return;

    next_pp_token();

    InternedString filename{};
    if (token == TOK_STRING_LITERAL) {
        filename = intern_string(unescape_string(lexer.text(), lexer.location()));
        next_pp_token();
    }

    if (token != '\n') return;

    lexer.lineno(line - 1);
    if (filename) {
        lexer.set_filename(filename);
    }
}

void Preprocessor::handle_error_directive() {
    auto& stream = message(Severity::ERROR, location());
    next_pp_token();
    auto begin = lexer.text().data();
    auto end = begin;
    while (token && token != '\n') {
        auto text = lexer.text();
        end = text.data() + text.length();
        next_pp_token();
    }

    stream << string_view(begin, end - begin) << '\n';
}

void Preprocessor::handle_include_directive() {
    next_pp_token();

    if (token != TOK_STRING_LITERAL) {
        unexpected_directive_token();
        skip_to_eol();
        return;
    }

    auto header_name = lexer.text();

    auto file = FileCache::it->search(header_name);
    if (!file) {
        message(Severity::ERROR, location()) << "cannot read file " << header_name << '\n';
        skip_to_eol();
        return;
    }

    next_pp_token();
    if (token != '\n') {
        unexpected_directive_token();
        skip_to_eol();
    }

    include_stack.push_back(lexer.location());

    auto matcher = lexer.new_matcher();
    matcher->buffer((char*) file->text.c_str(), file->text.length() + 1);

    lexer.push_matcher(matcher);
    lexer.lineno(1);
    lexer.set_filename(intern_string(file->path.string()));
}

void Preprocessor::handle_pragma_directive() {
    skip_to_eol();
}
