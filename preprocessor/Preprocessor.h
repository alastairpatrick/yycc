#ifndef PREPROCESSOR_PREPROCESSOR_H
#define PREPROCESSOR_PREPROCESSOR_H

#include "generated/IdentifierLexer.yy.h"
#include "generated/PPNumberLexer.yy.h"
#include "generated/PPTokenLexer.yy.h"
#include "Identifier.h"
#include "lexer/Location.h"
#include "lexer/Token.h"
#include "TextStream.h"

struct Preprocessor {
    explicit Preprocessor(bool preparse);
    void operator=(const Preprocessor&) = delete;

    void in(const Input& input);
    void buffer(string_view fragment);

    TokenKind next_token();

    string_view text() const {
        return lexer.text();
    }

    Fragment fragment() const {
        return lexer.fragment();
    }

    Location location() const {
        return lexer.location();
    }

    Identifier identifier() const;

private:
    TokenKind next_token_internal();
    TokenKind commit_token(TokenKind token, string_view text);
    void handle_directive();
    void handle_line_directive();
    void handle_error_directive();
    void handle_pragma_directive();
    void skip_to_eol();
    void require_eol();

    const bool preparse;
    TokenKind token;
    PPTokenLexer lexer;
    IdentifierLexer id_lexer;
    PPNumberLexer num_lexer;
    TextStream output;
    ostringstream output_stream;
};

#endif
