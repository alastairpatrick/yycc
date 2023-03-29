#ifndef LEXER_LOCATION_H
#define LEXER_LOCATION_H

#include "InternedString.h"

struct Location {
    size_t line{};
    size_t column{};

    // Filename might contain backslash escape sequences.
    InternedString filename = empty_interned_string;
};

#endif
