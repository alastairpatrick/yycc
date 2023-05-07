#include "IdentifierMap.h"
#include "Declaration.h"
#include "Message.h"

Declarator* IdentifierMap::lookup_declarator(const Identifier& identifier) const {
    for (auto scope: scopes) {
        InternedString identifier_string = scope == scopes.back() ? identifier.at_file_scope : identifier.text;

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
                                          string_view current_namespace_prefix,
                                          const Identifier& identifier_pair,
                                          DeclaratorDelegate* delegate,
                                          const Location& location,
                                          Declarator* primary) {
    InternedString identifier = identifier_pair.text;

    if (identifier->empty()) return new Declarator(declaration, type, identifier, delegate, location);

    if (add_scope == AddScope::FILE) {
        return add_declarator_internal(scopes.back(), declaration, type, current_namespace_prefix, identifier, delegate, location, primary);
    } else if (add_scope == AddScope::TOP) {
        return add_declarator_internal(scopes.front(), declaration, type, current_namespace_prefix, identifier, delegate, location, primary);
    } else {
        InternedString qualified_identifier = identifier;
        auto deepest_scope = scopes.front();
        for (; deepest_scope; deepest_scope = deepest_scope->parent) {
            if (deepest_scope->kind != ScopeKind::STRUCTURED) {
                break;
            }

            if (add_scope == AddScope::FILE_OR_BLOCK_QUALIFIED) {
                qualified_identifier = intern_string(*deepest_scope->prefix, *qualified_identifier);
            }
        }

        Declarator* result_declarator = add_declarator_internal(deepest_scope, declaration, type, current_namespace_prefix, qualified_identifier, delegate, location, primary);
        primary = result_declarator->primary;

        if (add_scope == AddScope::FILE_OR_BLOCK_UNQUALIFIED) {
            return result_declarator;
        }

        qualified_identifier = identifier;
        for (auto scope = scopes.front(); scope != deepest_scope; scope = scope->parent) {
            auto declarator = add_declarator_internal(scope, declaration, type, current_namespace_prefix, qualified_identifier, delegate, location, primary);
            if (scope == scopes.front()) result_declarator = declarator;

            qualified_identifier = intern_string(*scope->prefix, *qualified_identifier);
        }

        return result_declarator;
    }
}

Declarator* IdentifierMap::add_declarator_internal(Scope* scope,
                                                   const Declaration* declaration,
                                                   const Type* type,
                                                   string_view current_namespace_prefix,
                                                   InternedString identifier,
                                                   DeclaratorDelegate* delegate,
                                                   const Location& location,
                                                   Declarator* primary) {
    if (scope == scopes.back()) {
        if ((*identifier)[0] == ':') {
            identifier = intern_string(identifier->substr(2));
        } else {
            identifier = intern_string(current_namespace_prefix, *identifier);
        }
    }

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
    if (scopes.size()) {
        scope->parent = scopes.front();
    }
    scopes.push_front(scope);
}

Scope* IdentifierMap::pop_scope() {
    auto popped = scopes.front();
    scopes.pop_front();
    return popped;
}

IdentifierMap::IdentifierMap(bool preparse): preparse(preparse) {
    push_scope(new Scope(ScopeKind::FILE));
}

ScopeKind IdentifierMap::scope_kind() const {
    return scopes.front()->kind;
}
