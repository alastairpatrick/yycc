#ifndef SYMBOL_MAP_H
#define SYMBOL_MAP_H

#include "std.h"
#include "ASTNode.h"
#include "Type.h"

struct Decl;
struct Mystery;

struct SymbolMap {
    struct Scope {
        unordered_map<const string*, Decl*> declarations;
    };
    list<Scope> scopes;

    Decl* lookup_decl(TypeNameKind kind, const Identifier& identifier);
    const Type* lookup_type(TypeNameKind kind, const Identifier& identifier);
    void add_decl(TypeNameKind kind, Decl* decl);

    void push_scope();
    void pop_scope();

    SymbolMap();
};

#endif