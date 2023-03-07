#ifndef TOKEN_CONVERTER_H
#define TOKEN_CONVERTER_H

#include "Identifier.h"
#include "PPTokenLexer.yy.h"
#include "Location.h"
#include "Token.h"

#include "std.h"

struct TokenConverter: PPTokenLexer {
    TokenConverter(const reflex::Input& input): PPTokenLexer(input) {}

    Location location() const {
        return Location { lineno(), columno() + 1, current_filename };
    }

    size_t byte_offset() const {
        return matcher().first();
    }

    int lex();
    Identifier TokenConverter::identifier() const;

private:
    TokenKind lex_internal();
    void handle_directive();
    void handle_error_directive();
    void handle_line_directive();
    void handle_pragma_directive();
    void skip_to_eol();

    TokenKind token;

    unordered_set<string> filenames;
    const char* current_filename = "";
};

#endif
