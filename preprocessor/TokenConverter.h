#ifndef TOKEN_CONVERTER_H
#define TOKEN_CONVERTER_H

#include "Identifier.h"
#include "IdentifierLexer.yy.h"
#include "PPNumberLexer.yy.h"
#include "PPTokenLexerSource.h"
#include "Location.h"
#include "Token.h"

struct TokenConverter {
    explicit TokenConverter(const Input& input): source(input) {}
    void operator=(const TokenConverter&) = delete;

    int next_token();

    string_view text() const {
        return source.text();
    }

    Location location() const {
        return source.location();
    }

    Identifier TokenConverter::identifier() const;

private:
    TokenKind next_token_internal();
    void handle_directive();
    void handle_error_directive();
    void handle_line_directive();
    void handle_pragma_directive();
    void skip_to_eol();

    TokenKind token;

    PPTokenLexerSource source;
    IdentifierLexer id_lexer;
    PPNumberLexer num_lexer;
};

#endif
