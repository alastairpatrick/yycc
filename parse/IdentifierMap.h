#ifndef PARSE_IDENTIFIER_MAP_H
#define PARSE_IDENTIFIER_MAP_H

#include "ASTNode.h"
#include "Scope.h"
#include "Type.h"

enum class AddScope {
    FILE,
    TOP,
};

struct IdentifierMap {
    list<Scope*> scopes;

    IdentifierMap();
    void operator=(const IdentifierMap&) = delete;

    Declarator* lookup_declarator(const Identifier& identifier) const;
    Declarator* add_declarator(AddScope add_scope,
                               const Type* type,
                               const Identifier& identifier,
                               DeclaratorDelegate* delegate,
                               const Location& location,
                               Declarator* primary = nullptr);

    Declarator* add_declarator_internal(const Type* type,
                                        Scope* scope,
                                        const Identifier& identifier,
                                        DeclaratorDelegate* delegate,
                                        const Location& location,
                                        Declarator* primary = nullptr);

    void push_scope(Scope* scope);
    Scope* pop_scope();

    Scope* file_scope() const { return scopes.back(); }
    Scope* top_scope() const { return scopes.front(); }
    ScopeKind scope_kind() const;
};

#endif
