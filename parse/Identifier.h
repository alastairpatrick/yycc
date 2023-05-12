#ifndef LEX_IDENTIFIER_H
#define LEX_IDENTIFIER_H

#include "InternedString.h"

struct Identifier {
    InternedString usage_at_file_scope = empty_interned_string;   // result to handle expansion during preprocessing
    InternedString qualified = empty_interned_string;             // namespace prefixed, handles not applied
    InternedString text = empty_interned_string;                  // unmodified text of identifier token
    size_t position{};

    const char* c_str() const;
    bool empty() const { return text->empty(); }
};

ostream& operator<<(ostream& stream, const Identifier& identifier);

#endif
