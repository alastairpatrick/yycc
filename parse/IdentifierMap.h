#ifndef PARSE_IDENTIFIER_MAP_H
#define PARSE_IDENTIFIER_MAP_H

#include "ASTNode.h"
#include "Scope.h"
#include "Type.h"


struct IdentifierMap {
    const bool preparse;
    list<Scope> scopes;

    explicit IdentifierMap(bool preparse);
    void operator=(const IdentifierMap&) = delete;

    Declarator* lookup_declarator(const Identifier& identifier) const;
    Declarator* add_declarator(ScopeKind scope, const Declaration* declaration, const Type* type, const Identifier& identifier, const Location& location, Declarator* primary = nullptr);

    void push_scope(ScopeKind kind);
    void push_scope(Scope&& scope);
    Scope pop_scope();

    ScopeKind scope_kind() const;
};

#endif
