#ifndef AST_SYMBOL_MAP_H
#define AST_SYMBOL_MAP_H

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

    Declarator* lookup_declarator(const Identifier& identifier) const;
    const Type* lookup_type(const Identifier& identifier) const;
    void add_declarator(Declarator* declarator);

    void push_scope();
    void pop_scope();
};

#endif