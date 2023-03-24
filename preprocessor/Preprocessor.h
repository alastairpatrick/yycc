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

    void set_input(const Input& input);
    void set_input(const Fragment& fragment);

    TokenKind next_token();

    Fragment fragment() const {
        return lexer.fragment();
    }

    Location location() const {
        return lexer.location();
    }

    Identifier identifier() const;

protected:
    TokenKind next_token_internal();
    virtual void handle_directive() = 0;
    void skip_to_eol();
    void require_eol();

    TokenKind token;

private:
    void handle_line_directive();

    PPTokenLexerSource lexer;
    IdentifierLexer id_lexer;
    PPNumberLexer num_lexer;
};

#endif
