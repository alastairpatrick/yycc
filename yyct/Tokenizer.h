#ifndef TOKENIZER_H
#define TOKENIZER_H
#include "std.h"

#include "PPTokenLexer.yy.h"
#include "Token.h"

using reflex::Input;

struct Tokenizer {
    struct Identifier {
        string text;
        size_t frequency{};
        size_t index{};
    };

    typedef unordered_map<string, Identifier> IdentifierMap;
    IdentifierMap identifier_map;

    int current_column = 0;
    int current_line = 1;

    void histogram(const string& source);
    void write_identifiers(ostream& stream);
    void align_token(ostream& stream, const Phase3Lexer& lexer);
    void write_token(ostream& stream, const Phase3Lexer& lexer);
    void rewrite(ostream& stream, const string& source);
    void process(ostream& stream, const string& source);
};


#endif
