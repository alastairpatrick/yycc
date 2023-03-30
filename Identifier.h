#ifndef IDENTIFIER_H
#define IDENTIFIER_H

#include "InternedString.h"

struct Identifier {
    Identifier() : name(empty_interned_string) {}
    explicit Identifier(string_view s): name(intern_string(s)) {}
    InternedString name{};
};

inline ostream& operator<<(ostream& stream, const Identifier& identifier) {
    return stream << *identifier.name;
}

#endif
