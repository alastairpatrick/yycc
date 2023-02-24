#ifndef IDENTIFIER_H
#define IDENTIFIER_H

#include "CompileContext.h"

struct Identifier {
    InternedString name{};

    // This is a byte offset in the preprocessed source text. It is used to lookup declarations in the symbol map as they were earlier in the source.
    size_t byte_offset = 0;
};

inline ostream& operator<<(ostream& stream, const Identifier& identifier) {
    return stream << *identifier.name;
}

#endif
