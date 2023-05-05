#ifndef PARSE_SCOPE_H
#define PARSE_SCOPE_H

#include "InternedString.h"

struct Declarator;
struct StructuredType;

enum class ScopeKind {
    FILE,
    BLOCK,
    PROTOTYPE,
    STRUCTURED,
};

struct Scope {
    ScopeKind kind;
    const StructuredType* type{};
    vector<Declarator*> declarators;
    unordered_map<InternedString, Declarator*> declarator_map;

    explicit Scope(ScopeKind kind): kind(kind) {}
};

#endif
