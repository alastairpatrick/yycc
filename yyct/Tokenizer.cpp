#include "Tokenizer.h"

constexpr int horz_tab_size = 4;

static bool should_inline(const Tokenizer::Identifier& identifier) {
    size_t internal_size = identifier.frequency * identifier.text.length();
    size_t external_size = identifier.frequency * 3 + identifier.text.length() + 1;  // approximate - assumes two byte varuint index

    return internal_size < external_size;
}

static void write_varuint(ostream& stream, size_t v) {
    while (v >= 0x80) {
        uint8_t b = (v & 0x7F) | 0x80;
        v >>= 7;
        stream.write((const char*) &b, sizeof(b));
    }
    uint8_t b = v;
    stream.write((const char*) &b, sizeof(b));
}

void Tokenizer::write_identifiers(ostream& stream) {
    vector<Identifier*> ordered;
    ordered.reserve(identifier_map.size());
    for (auto& kv : identifier_map) {
        ordered.push_back(&kv.second);
    }
    
    std::sort(ordered.begin(), ordered.end(), [](const Identifier* l, const Identifier* r) {
        return l->frequency > r->frequency || l->frequency == r->frequency && l->text < r->text;
    });

    size_t index = 0;
    for (auto identifier : ordered) {
        identifier->index = index;

        if (should_inline(*identifier)) continue;

        ++index;
        stream << identifier->text << '\0';
    }
}

void Tokenizer::histogram(const string& source) {
    Input input(source);
    Phase3Lexer lexer(input);
    auto& matcher = lexer.matcher();

    TokenKind kind;
    do {
        kind = TokenKind(lexer.lex());

        if (kind == TOK_IDENTIFIER) {
            auto text = lexer.str();
            auto& identifier = identifier_map[text];
            identifier.text = text;
            ++identifier.frequency;
        }
    } while (kind);
}

void Tokenizer::align_token(ostream& stream, const Phase3Lexer& lexer) {
    int target_line = lexer.lineno();
    int target_column = lexer.columno();

    while (target_line - current_line >= 1) {
        stream << '\n';
        current_column = 0;
        current_line += 1;
    }

    while (target_column - current_column >= horz_tab_size) {
        stream << '\t';
        current_column += horz_tab_size;
    }

    while (target_column - current_column >= 1) {
        stream << ' ';
        current_column += 1;
    }
}

void Tokenizer::write_token(ostream& stream, const Phase3Lexer& lexer) {
    align_token(stream, lexer);
    stream.write(lexer.matcher().begin(), lexer.size());
    current_column += lexer.size();
}

void Tokenizer::rewrite(ostream& stream, const string& source) {
    Input input(source);
    Phase3Lexer lexer(input);
    auto& matcher = lexer.matcher();

    TokenKind token;
    do {
        token = TokenKind(lexer.lex());

        switch (token) {
              default: {
                write_token(stream, lexer);
                break;
            } case TOK_IDENTIFIER: {
                auto& identifier = identifier_map[lexer.str()];
                if (should_inline(identifier)) {
                    write_token(stream, lexer);
                } else {
                    align_token(stream, lexer);
                    stream << '\x1B';
                    write_varuint(stream, identifier.index);
                    current_column += identifier.text.length();
                }
                break;
            } case TOK_PP_INCLUDE:
              case TOK_PP_DEFINE:
              case TOK_PP_IF:
              case TOK_PP_IFDEF:
              case TOK_PP_IFNDEF:
              case TOK_PP_ELIF:
              case TOK_PP_ELSE:
              case TOK_PP_ENDIF: {
                align_token(stream, lexer);
                stream.write((const char*) &token, 1);
                current_column += lexer.size();
                break;
            } case '\n':
              case TOK_PP_COMMENT: {
                break;
            }
        }
    } while (token);
}

void Tokenizer::process(ostream& stream, const string& source) {
    histogram(source);
    write_identifiers(stream);
    rewrite(stream, source);
}
