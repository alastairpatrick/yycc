#ifndef PARSE_IDENTIFIER_MAP_H
#define PARSE_IDENTIFIER_MAP_H

#include "ASTNode.h"
#include "Scope.h"
#include "Type.h"

struct Declarator;
struct Mystery;

struct IdentifierMap {
    explicit IdentifierMap(bool preparse);
    void operator=(const IdentifierMap&) = delete;

    list<Scope> scopes;
    const bool preparse;

    Declarator* lookup_declarator(const Identifier& identifier) const;
    const Type* lookup_type(const Identifier& identifier) const;
    void add_declarator(Declarator* declarator);
    Declarator* add_declarator_to_scope(Scope& scope, Declarator* declarator);

    void push_scope();
    Scope pop_scope();
};

#endif