#ifndef IDENTIFIER_H
#define IDENTIFIER_H

#include "InternedString.h"

enum class TypeNameKind {
    ENUM,
    ORDINARY,
    STRUCT,
    UNION,
    NUM
};

struct Identifier {
    explicit Identifier(string_view s): name(intern_string(s)) {}
    InternedString name{};
};

inline ostream& operator<<(ostream& stream, const Identifier& identifier) {
    return stream << *identifier.name;
}

#endif
