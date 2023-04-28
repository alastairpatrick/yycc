#ifndef PREPROCESS_PREPROCESSOR_H
#define PREPROCESS_PREPROCESSOR_H

#include "generated/IdentifierLexer.yy.h"
#include "generated/PPNumberLexer.yy.h"
#include "generated/PPTokenLexer.yy.h"
#include "lex/Identifier.h"
#include "lex/Location.h"
#include "lex/Token.h"
#include "TextStream.h"

struct Preprocessor {
    const bool preparse;
    TokenKind token;
    Fragment fragment;    
    vector<Location> include_stack;

    explicit Preprocessor(bool preparse);
    Preprocessor(string_view input, bool preparse);
    void operator=(const Preprocessor&) = delete;

    void in(const Input& input);
    TokenKind next_token();
    TokenKind next_pp_token();

    string_view text() const {
        return lexer.text();
    }

    Location location() const {
        return lexer.location();
    }

    Identifier identifier() const {
        return lexer.identifier();
    }

    void skip_to_eol();
    void require_eol();
    void unexpected_directive_token();

    string_view output();

private:
    TokenKind commit_token(TokenKind token, string_view text);
    bool handle_directive();
    void handle_line_directive();
    void handle_error_directive();
    void handle_include_directive();
    void handle_pragma_directive();
    void handle_type_directive();

    PPTokenLexer lexer;
    IdentifierLexer id_lexer;
    PPNumberLexer num_lexer;

    size_t pending_line{};
    size_t pending_column{};

    TextStream text_stream;
    strstream string_stream;
};

#endif
