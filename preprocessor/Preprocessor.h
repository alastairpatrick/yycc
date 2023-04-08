#ifndef PREPROCESSOR_PREPROCESSOR_H
#define PREPROCESSOR_PREPROCESSOR_H

#include "generated/IdentifierLexer.yy.h"
#include "generated/PPNumberLexer.yy.h"
#include "generated/PPTokenLexer.yy.h"
#include "lexer/Identifier.h"
#include "lexer/Location.h"
#include "lexer/Token.h"
#include "TextStream.h"

struct Preprocessor {
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

    bool mark_root() const;

    string_view output();

    const bool preparse;
    TokenKind token;
    Fragment fragment;

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

    vector<Location> include_stack;

    TextStream text_stream;
    strstream string_stream;
};

#endif
