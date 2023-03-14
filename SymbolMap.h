#ifndef SYMBOL_MAP_H
#define SYMBOL_MAP_H

#include "ASTNode.h"
#include "Type.h"

struct Declarator;
struct Mystery;

struct SymbolMap {
    explicit SymbolMap(bool preparse);
    void operator=(const SymbolMap&) = delete;

    struct Scope {
        unordered_map<InternedString, Declarator*> declarators;
    };
    list<Scope> scopes;
    const bool preparse;

    Declarator* lookup_declarator(TypeNameKind kind, const Identifier& identifier);
    const Type* lookup_type(TypeNameKind kind, const Identifier& identifier);
    void add_declarator(TypeNameKind kind, Declarator* declarator);

    void push_scope();
    void pop_scope();
};

#endif