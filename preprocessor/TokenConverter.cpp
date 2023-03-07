#include "TokenConverter.h"

#include "Identifier.h"
#include "IdentifierLexer.yy.h"
#include "PPNumberLexer.yy.h"

using reflex::Input;

string unescape_string(const char* text, size_t capacity_hint, const Location& location);

int TokenConverter::lex() {
    for (;;) {
        lex_internal();
        switch (token) {
              default: {
                return token;
            } case TOK_IDENTIFIER: {
                IdentifierLexer id_lexer(Input(matcher().begin(), size()));
                token = TokenKind(id_lexer.lex());
                return id_lexer.size() == size() ? token : TOK_IDENTIFIER;
            } case TOK_PP_NUMBER: {
                PPNumberLexer num_lexer(Input(matcher().begin(), size()));
                token = TokenKind(num_lexer.lex());
                return num_lexer.size() == size() ? token : TOK_PP_NUMBER;
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
    lex_internal();
    switch (token) {
          case TOK_PP_ERROR: {
            handle_error_directive();
            break;
        } case TOK_PP_LINE: {
            handle_line_directive();
            break;
        } case TOK_PP_PRAGMA: {
            skip_to_eol();
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
    lex_internal();
    auto begin = matcher().begin();
    auto end = begin;
    while (token && token != '\n') {
        end = matcher().end();
        lex_internal();
    }

    stream << string_view(begin, end - begin) << '\n';
}

void TokenConverter::handle_line_directive() {
    lex_internal();

    if (token != TOK_PP_NUMBER) return;

    char* end;
    auto line = strtoll(text(), &end, 10);
    if (line <= 0 || end != matcher().end()) return;

    lex_internal();

    string filename;
    if (token == TOK_STRING_LITERAL) {
        filename = unescape_string(text(), size(), location());
        lex_internal();
    }

    if (token != '\n') return;

    lineno(line - 1);
    if (!filename.empty()) {
        current_filename = filenames.insert(filename).first->c_str();
    }
}

void TokenConverter::handle_pragma_directive() {
    skip_to_eol();
}

void TokenConverter::skip_to_eol() {
    while (token && token != '\n') {
        lex_internal();
    }
}

TokenKind TokenConverter::lex_internal() {
    return token = TokenKind(PPTokenLexer::lex());
}

Identifier TokenConverter::identifier() const
{
    Identifier result;
    result.name = InternString(string_view(matcher().begin(), matcher().size()));
    result.byte_offset = byte_offset();
    return result;
}