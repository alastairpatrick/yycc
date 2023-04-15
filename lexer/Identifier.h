#ifndef LEXER_IDENTIFIER_H
#define LEXER_IDENTIFIER_H

#include "InternedString.h"

struct Identifier {
    Identifier() : name(empty_interned_string) {}
    explicit Identifier(string_view s): name(intern_string(s)) {}
    InternedString name{};
};

inline bool operator==(const Identifier& a, const Identifier& b) {
    return a.name == b.name;
}

inline bool operator!=(const Identifier& a, const Identifier& b) {
    return a.name != b.name;
}

inline ostream& operator<<(ostream& stream, const Identifier& identifier) {
    return stream << *identifier.name;
}

#endif
