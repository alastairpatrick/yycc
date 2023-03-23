#ifndef PREPROCESSOR_PREPROCESSOR_H
#define PREPROCESSOR_PREPROCESSOR_H

#include "generated/IdentifierLexer.yy.h"
#include "generated/PPNumberLexer.yy.h"
#include "Identifier.h"
#include "lexer/Location.h"
#include "lexer/PPTokenLexerSource.h"
#include "lexer/Token.h"

struct Preprocessor {
    void operator=(const Preprocessor&) = delete;

    TokenKind next_token();

    string_view text() const {
        return source.text();
    }

    Location location() const {
        return source.location();
    }

    Identifier identifier() const;

protected:
    explicit Preprocessor(string_view view): source(view) {
    }

    TokenKind next_token_internal();
    virtual void handle_directive() = 0;
    void skip_to_eol();

    TokenKind token;

private:
    void handle_line_directive();

    PPTokenLexerSource source;
    IdentifierLexer id_lexer;
    PPNumberLexer num_lexer;
};

#endif
