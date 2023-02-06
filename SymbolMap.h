#ifndef SYMBOL_MAP_H
#define SYMBOL_MAP_H

#include "std.h"
#include "Type.h"

struct Decl;

struct SymbolMap {
    struct Scope {
        unordered_map<const string*, Decl*> declarations;
    };
    list<Scope> scopes;

    Decl* lookup_decl(TypeNameKind kind, const string* name) const;
    const Type* lookup_type(TypeNameKind kind, const string* name) const;
    void add_decl(TypeNameKind kind, const string* name, Decl* decl);

    void push_scope();
    void pop_scope();

    SymbolMap();
};

#endif