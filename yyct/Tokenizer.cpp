#include "Tokenizer.h"

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
    PPTokenLexer lexer(input);
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

// Character \n adjusts vertically by 1
// Characters 1..8 adjust vertically by 2..9
const int max_vert_adjust = 9;

// Characters ('\n', ' '] are space characters with varying column number adjustment
const int max_horz_adjust = ' ' - '\n' - 1;


static char horz_space_char(int columns) {
    assert(columns <= max_horz_adjust);
    return ' ' - (columns - 1);
}

static char vert_space_char(int lines) {
    assert(2 <= lines && lines <= max_vert_adjust);
    return lines - 1;
}

void Tokenizer::align_token(ostream& stream, const PPTokenLexer& lexer) {
    int target_line = lexer.lineno();
    int target_column = lexer.columno();

    while (target_line - 1 - current_line >= max_vert_adjust) {
        stream << vert_space_char(max_vert_adjust);
        current_line += max_vert_adjust;
        current_column = 0;
    }

    if (target_line - 1 - current_line >= 2) {
        stream << vert_space_char(target_line - 1 - current_line);
        current_line = target_line - 1;
        current_column = 0;
    }

    while (target_line > current_line) {
        stream << '\n';
        ++current_line;
        current_column = 0;
    }

    while (target_column - current_column > max_horz_adjust) {
        stream << horz_space_char(max_horz_adjust);
        current_column += max_horz_adjust;
    }

    if (target_column > current_column) {
        stream << horz_space_char(target_column - current_column);
        current_column = target_column;
    }
}

void Tokenizer::write_token(ostream& stream, const char* str, size_t size) {
    stream.write(str, size);
    current_column += size;
}

void Tokenizer::rewrite(ostream& stream, const string& source) {
    Input input(source);
    PPTokenLexer lexer(input);
    auto& matcher = lexer.matcher();

    TokenKind token;
    do {
        token = TokenKind(lexer.lex());
        if (token != '\n') {
            align_token(stream, lexer);
        }

        switch (token) {
              default: {
                write_token(stream, lexer.matcher().begin(), lexer.size());
                break;
            } case TOK_IDENTIFIER: {
                auto& identifier = identifier_map[lexer.str()];
                if (should_inline(identifier)) {
                    write_token(stream, lexer.matcher().begin(), lexer.size());
                } else {
                    stream << '\0';
                    write_varuint(stream, identifier.index);
                    current_column += identifier.text.length();
                }
                break;
            } case TOK_PP_DEFINE: {
                write_token(stream, "d", 1);
                break;
            } case '\n': {
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
