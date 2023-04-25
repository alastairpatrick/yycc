#ifndef LEX_IDENTIFIER_H
#define LEX_IDENTIFIER_H

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

ostream& operator<<(ostream& stream, const Identifier& identifier);

const char* identifier_name(const Identifier& identifier);

#endif
