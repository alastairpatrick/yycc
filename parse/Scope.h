#ifndef PARSE_SCOPE_H
#define PARSE_SCOPE_H

#include "InternedString.h"

struct Declarator;
struct StructuredType;

struct Scope {
    const StructuredType* type{};
    vector<Declarator*> declarators;
    unordered_map<InternedString, Declarator*> declarator_map;
};

#endif
