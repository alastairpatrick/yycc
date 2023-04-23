#ifndef INTERNED_STRING_H
#define INTERNED_STRING_H

// Interned strings:
//  * are always null itself or terminated by a null character
//  * if their characters and length are equal, their addresses are the same, allowing constant time equality test

typedef const string_view* InternedString;

extern InternedString empty_interned_string;

InternedString intern_string(string_view source);

#endif
