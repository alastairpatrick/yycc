#ifndef LEX_IDENTIFIER_H
#define LEX_IDENTIFIER_H

#include "InternedString.h"

struct Identifier {
    InternedString name{};

    Identifier() : name(empty_interned_string) {}
    explicit Identifier(string_view s): name(intern_string(s)) {}
    explicit Identifier(InternedString s): name(s) {}

    const char* c_str() const;
};

ostream& operator<<(ostream& stream, const Identifier& identifier);

#endif
