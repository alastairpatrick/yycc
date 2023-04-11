#ifndef PARSER_IDENTIFIER_MAP_H
#define PARSER_IDENTIFIER_MAP_H

#include "ASTNode.h"
#include "Type.h"

struct Declarator;
struct Mystery;

struct IdentifierMap {
    explicit IdentifierMap(bool preparse);
    void operator=(const IdentifierMap&) = delete;

    struct Scope {
        unordered_map<InternedString, Declarator*> declarators;
    };
    list<Scope> scopes;
    const bool preparse;

    Declarator* lookup_declarator(const Identifier& identifier) const;
    const Type* lookup_type(const Identifier& identifier) const;
    bool add_declarator(Declarator* declarator);
    bool add_declarator_to_scope(Scope& scope, Declarator* declarator);

    void push_scope();
    Scope pop_scope();
};

struct ResolutionContext {
    ResolutionContext(const IdentifierMap& identifiers);
    const IdentifierMap& identifiers;
};

#endif