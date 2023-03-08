#ifndef TOKEN_CONVERTER_H
#define TOKEN_CONVERTER_H

#include "Identifier.h"
#include "IdentifierLexer.yy.h"
#include "PPNumberLexer.yy.h"
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

    int next_token();
    Identifier TokenConverter::identifier() const;

private:
    TokenKind next_token_internal();
    void handle_directive();
    void handle_error_directive();
    void handle_line_directive();
    void handle_pragma_directive();
    void skip_to_eol();

    TokenKind token;

    unordered_set<string> filenames;
    const char* current_filename = "";

    IdentifierLexer id_lexer;
    PPNumberLexer num_lexer;
};

#endif
