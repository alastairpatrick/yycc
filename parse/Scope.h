#ifndef PARSE_SCOPE_H
#define PARSE_SCOPE_H

#include "ASTNode.h"
#include "InternedString.h"

struct Declarator;
struct Identifier;
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
    InternedString namespace_prefix = empty_interned_string;

    explicit Scope(ScopeKind kind);
    Scope(ScopeKind kind, InternedString namespace_prefix);

    Declarator* lookup_declarator(const Identifier& identifier) const;
    Declarator* lookup_member(const Identifier& identifier) const;
};

#endif
