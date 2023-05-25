#ifndef PREPROCESS_PREPROCESSOR_H
#define PREPROCESS_PREPROCESSOR_H

#include "generated/IdentifierLexer.yy.h"
#include "generated/PPNumberLexer.yy.h"
#include "generated/PPTokenLexer.yy.h"
#include "lex/Location.h"
#include "lex/Token.h"
#include "TextStream.h"
#include "parse/Identifier.h"

struct IncludeContext {
    Location location;
    InternedString current_namespace_prefix;
    unordered_map<InternedString, InternedString> namespace_handles;
};

struct Preprocessor {
    const bool preparse;
    size_t position;
    TokenKind token;
    Identifier identifier;
    vector<IncludeContext> include_stack;
    InternedString current_namespace_prefix;
    unordered_map<InternedString, InternedString> namespace_handles;

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

    void skip_to_eol();
    void require_eol();
    void unexpected_directive_token();

    string_view output();

private:
    void commit_token();
    void begin_pass_through_directive(const Location& pound_location);
    bool handle_directive(const Location& pound_location);
    void handle_line_directive();
    void handle_error_directive();
    void handle_include_directive();
    void handle_pragma_directive();

    void reset_namespace();
    void add_keyword(string_view id);
    void handle_namespace_directive();
    void handle_using_directive();
    InternedString evaluate_identifier(InternedString text) const;

    PPTokenLexer lexer;
    IdentifierLexer id_lexer;
    PPNumberLexer num_lexer;

    size_t pending_line{};
    size_t pending_column{};

    bool auto_commit_tokens{};
    TextStream text_stream;
    strstream string_stream;
};

#endif
