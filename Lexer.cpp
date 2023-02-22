#include "Location.h"
#include "Token.h"

struct Token {
    TokenKind kind{};
    const char* text{};
    Location location;
};

enum class SpaceKind {
    ALL,
    EXCEPT_NEW_LINE,
};

struct Lexer {
    Lexer(const char* begin, const char* end): p(begin), end(end) {}

    virtual void next_token();

    Token token;
private:
    int c;
    const char* p;
    const char* end;

    void next_char();
    void skip_space(SpaceKind kind);
    void complete_directive(TokenKind kind, const char* seen, const char* completion);
    void recover(char recovery_char);
};

void Lexer::next_token() {
    switch (c) {
    case '#':
        goto sharp;
    }

sharp:
    next_char();
    skip_space(SpaceKind::EXCEPT_NEW_LINE);
    switch (c) {
    case 'd': complete_directive(TOK_PP_DEFINE, "d", "efine");
    }

}

void Lexer::next_char() {
    c = (p == end) ? EOF : *p++;
}

void Lexer::skip_space(SpaceKind kind) {
    for (;;) {
        switch (c) {
        case ' ':
        case '\f':
        case '\r':
        case '\t':
        case '\v':
            next_char();
            break;
        case '\n':
            if (kind == SpaceKind::EXCEPT_NEW_LINE) return;
            next_char();
        default:
            return;
        }
    }
}

void Lexer::complete_directive(TokenKind kind, const char* seen, const char* completion) {
    auto* p = completion;
    while (*p) {
        next_char();
        if (c != *p) {
            cerr << "Unexpected token resembling '" << seen << completion << "'\n";
            recover('\n');
            token.kind = TOK_PP_EMPTY;
        }
    }
}

void Lexer::recover(char recovery_char) {
    while (c != EOF && c != recovery_char) {
        next_char();
    }
}

