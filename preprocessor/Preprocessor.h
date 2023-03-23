#ifndef PREPROCESSOR_PREPROCESSOR_H
#define PREPROCESSOR_PREPROCESSOR_H

#include "generated/IdentifierLexer.yy.h"
#include "generated/PPNumberLexer.yy.h"
#include "Identifier.h"
#include "lexer/Location.h"
#include "lexer/PPTokenLexerSource.h"
#include "lexer/Token.h"

struct Preprocessor {
    explicit Preprocessor(string_view view): source(view) {
    }

    void operator=(const Preprocessor&) = delete;

    int next_token();

    string_view text() const {
        return source.text();
    }

    Location location() const {
        return source.location();
    }

    Identifier identifier() const;

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
