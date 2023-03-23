#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "generated/PPTokenLexer.yy.h"
#include "lexer/Token.h"

struct Tokenizer {
    Tokenizer() = default;
    Tokenizer(const Tokenizer&) = delete;
    void operator=(const Tokenizer&) = delete;

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
    void align_token(ostream& stream, const PPTokenLexer& lexer);
    void write_token(ostream& stream, const char* str, size_t size);
    void rewrite(ostream& stream, const string& source);
    void process(ostream& stream, const string& source);
};


#endif
