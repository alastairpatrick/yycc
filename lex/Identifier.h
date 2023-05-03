#ifndef LEX_IDENTIFIER_H
#define LEX_IDENTIFIER_H

#include "InternedString.h"

struct Identifier {
    InternedString name = empty_interned_string;
    const char* c_str() const;
};

ostream& operator<<(ostream& stream, const Identifier& identifier);

#endif
