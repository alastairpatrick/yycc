#ifndef INTERNED_STRING_H
#define INTERNED_STRING_H

// Interned strings:
//  * are always null itself or terminated by a null character
//  * if their characters and length are equal, their addresses are the same, allowing constant time equality test

typedef pair<string_view, string_view> InternedStringPair;
typedef const string_view* InternedString;

struct InternedStringPairHash {
    size_t operator()(const InternedStringPair& pair) const;
};

struct InternedStringPairKeyEqual {
    bool operator()(const InternedStringPair& a, const InternedStringPair& b) const;
};

extern InternedString empty_interned_string;

InternedString intern_string(string_view first, string_view second = {});
const char* c_str(InternedString str);

#endif
