#ifndef INTERNED_STRING_H
#define INTERNED_STRING_H

#include "std.h"

// Interned strings have the property that if their string values are equal, their addresses are the same,
// allowing constant time equality test.

typedef const string_view* InternedString;

extern InternedString EmptyInternedString;

InternedString InternString(string_view source);
ostream& operator<<(ostream& stream, InternedString s);

#endif
