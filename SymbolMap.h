#ifndef SYMBOL_MAP_H
#define SYMBOL_MAP_H

#include "ASTNode.h"
#include "Type.h"

struct Decl;
struct Mystery;

struct SymbolMap {
    explicit SymbolMap(bool preparse);
    void operator=(const SymbolMap&) = delete;

    struct Scope {
        unordered_map<InternedString, Decl*> declarations;
    };
    list<Scope> scopes;
    const bool preparse;

    Decl* lookup_decl(TypeNameKind kind, const Identifier& identifier);
    const Type* lookup_type(TypeNameKind kind, const Identifier& identifier);
    void add_decl(TypeNameKind kind, Decl* decl);

    void push_scope();
    void pop_scope();
};

#endif