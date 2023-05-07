#include "IdentifierMap.h"
#include "Declaration.h"
#include "Message.h"

Declarator* IdentifierMap::lookup_declarator(const Identifier& identifier) const {
    for (auto scope = top_scope; scope; scope = scope->parent) {
        InternedString identifier_string = scope == file_scope ? identifier.at_file_scope : identifier.text;

        auto it = scope->declarator_map.find(identifier_string);
        if (it != scope->declarator_map.end()) {
            // Note that this intentionally does _not_ always return the primary. For reporting errors
            // it is better to return a declarator that is currently in scope. The primary declarator
            // might not be in scope, e.g. it has extern storage class and block scope.
            return it->second;
        }
    }

    return nullptr;
}

Declarator* IdentifierMap::add_declarator(AddScope add_scope,
                                          const Declaration* declaration,
                                          const Type* type,
                                          const Identifier& identifier,
                                          DeclaratorDelegate* delegate,
                                          const Location& location,
                                          Declarator* primary) {
    
    InternedString identifier_string{};
    if (add_scope == AddScope::FILE || top_scope == file_scope) {
        identifier_string = identifier.at_file_scope;
    } else {
        identifier_string = identifier.text;
    }

    if (identifier_string->empty()) return new Declarator(declaration, type, identifier_string, delegate, location);

    if (add_scope == AddScope::FILE) {
        return add_declarator_internal(file_scope, declaration, type, identifier_string, delegate, location, primary);
    } else if (add_scope == AddScope::TOP) {
        return add_declarator_internal(top_scope, declaration, type, identifier_string, delegate, location, primary);
    } else {
        InternedString qualified_identifier = identifier_string;
        auto scope = top_scope;
        for (; scope; scope = scope->parent) {
            if (scope->kind != ScopeKind::STRUCTURED) {
                break;
            }

            if (add_scope == AddScope::FILE_OR_BLOCK_QUALIFIED) {
                qualified_identifier = intern_string(*scope->prefix, *qualified_identifier);
            }
        }

        Declarator* declarator = add_declarator_internal(scope, declaration, type, qualified_identifier, delegate, location, primary);
        primary = declarator->primary;

        if (add_scope == AddScope::FILE_OR_BLOCK_QUALIFIED && scope != top_scope) {
            declarator = add_declarator_internal(top_scope, declaration, type, identifier_string, delegate, location, primary);
        }

        return declarator;
    }
}

Declarator* IdentifierMap::add_declarator_internal(Scope* scope,
                                                   const Declaration* declaration,
                                                   const Type* type,
                                                   InternedString identifier,
                                                   DeclaratorDelegate* delegate,
                                                   const Location& location,
                                                   Declarator* primary) {

    Declarator* new_declarator{};
    Declarator* existing_declarator{};

    auto it = scope->declarator_map.find(identifier);
    if (it != scope->declarator_map.end()) {
        existing_declarator = it->second;
        if (!existing_declarator->type) {
            new_declarator = existing_declarator;
            assert(new_declarator->identifier == identifier);

            new_declarator->declaration = declaration;
            new_declarator->type = type;
            new_declarator->delegate = delegate;
            new_declarator->location = location;
        }
    }

    if (!new_declarator) {
        new_declarator = new Declarator(declaration, type, identifier, delegate, location);

        if (existing_declarator) {
            assert(!primary || existing_declarator->primary == primary);

            new_declarator->primary = existing_declarator->primary;
            new_declarator->next = existing_declarator->next;
            existing_declarator->next = new_declarator;

        } else {
            existing_declarator = new_declarator;
            scope->declarator_map[identifier] = new_declarator;

            if (primary) {
                new_declarator->primary = primary;
                new_declarator->next = primary->next;
                primary->next = new_declarator;
            }
        }
    }

    if (new_declarator->type) {
        scope->declarators.push_back(new_declarator);
    }

    return new_declarator;
}

void IdentifierMap::push_scope(Scope* scope) {
    scope->parent = top_scope;
    top_scope = scope;
}

Scope* IdentifierMap::pop_scope() {
    auto popped = top_scope;
    top_scope = top_scope->parent;
    return popped;
}

IdentifierMap::IdentifierMap(bool preparse): preparse(preparse) {
    file_scope = new Scope(ScopeKind::FILE);
    push_scope(file_scope);
}

ScopeKind IdentifierMap::scope_kind() const {
    return top_scope->kind;
}
