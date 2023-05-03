#ifndef LEX_IDENTIFIER_H
#define LEX_IDENTIFIER_H

#include "InternedString.h"

struct Identifier {
    InternedString text = empty_interned_string;
    const char* c_str() const;
    bool empty() const { return text->empty(); }
};

ostream& operator<<(ostream& stream, const Identifier& identifier);

#endif
