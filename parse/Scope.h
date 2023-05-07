#ifndef PARSE_SCOPE_H
#define PARSE_SCOPE_H

#include "ASTNode.h"
#include "InternedString.h"

struct Declarator;
struct StructuredType;

enum class ScopeKind {
    FILE,
    BLOCK,
    PROTOTYPE,
    STRUCTURED,
};

struct Scope: ASTNode {
    ScopeKind kind;
    Scope* parent{};
    const StructuredType* type{};
    vector<Declarator*> declarators;
    unordered_map<InternedString, Declarator*> declarator_map;
    InternedString prefix = empty_interned_string;

    explicit Scope(ScopeKind kind): kind(kind) {}

    Scope(ScopeKind kind, string_view identifier)
        : kind(kind), prefix(intern_string(identifier, "::")) {
    }
};

#endif
