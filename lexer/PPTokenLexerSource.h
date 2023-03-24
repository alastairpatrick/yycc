#ifndef PP_TOKEN_LEXER_STREAM_H
#define PP_TOKEN_LEXER_STREAM_H

#include "Fragment.h"
#include "generated/PPTokenLexer.yy.h"
#include "Location.h"
#include "Token.h"

struct PPTokenLexerSource {
    void set_input(const Input& input);
    void set_input(const Fragment& fragment);

    void operator=(const PPTokenLexerSource&) = delete;

    TokenKind next_token() {
        return TokenKind(lexer.next_token());
    }

    // Returns valid Fragment only if input is a Fragment.
    Fragment fragment() const {
        return Fragment(lexer.matcher().first(), lexer.size());
    }

    Input token_input() const {
        return Input(lexer.matcher().begin(), lexer.size());
    }

    Location location() const {
        return Location { lexer.lineno(), lexer.columno() + 1, current_filename };
    }

    void set_lineno(size_t line) {
        lexer.lineno(line);
    }

    void set_filename(string&& filename);

private:
    PPTokenLexer lexer;

    unordered_set<string> filenames;
    string_view current_filename;
};

#endif

