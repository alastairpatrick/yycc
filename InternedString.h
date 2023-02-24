#ifndef INTERNED_STRING_H
#define INTERNED_STRING_H

// Interned strings have the property that if their string values are equal, their addresses are the same,
// allowing constant time equality test.

typedef const string_view* InternedString;

extern InternedString EmptyInternedString;

InternedString InternString(string_view source);
std::ostream& operator<<(std::ostream& stream, InternedString s);

#endif
