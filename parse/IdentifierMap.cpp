#include "IdentifierMap.h"
#include "Declaration.h"
#include "Message.h"

Declarator* IdentifierMap::lookup_declarator(const Identifier& identifier) const {
    for (auto& scope : scopes) {
        auto it = scope.declarator_map.find(identifier.text);
        if (it != scope.declarator_map.end()) {
            // Note that this intentionally does _not_ always return the primary. For reporting errors
            // it is better to return a declarator that is currently in scope. The primary declarator
            // might not be in scope, e.g. it has extern storage class and block scope.
            return it->second;
        }
    }

    return nullptr;
}

const Type* IdentifierMap::lookup_type(const Identifier& identifier) const {
    auto declarator = lookup_declarator(identifier);
    if (!declarator) return nullptr;

    return declarator->to_type();
}

Declarator* IdentifierMap::add_declarator(IdentifierScope add_scope, const Declaration* declaration, const Type* type, const Identifier& identifier, const Location& location, Declarator* primary) {
    if (identifier.empty()) return new Declarator(declaration, type, identifier, location);

    Scope& scope = add_scope == IdentifierScope::FILE ? scopes.back() : scopes.front();
    Declarator* new_declarator{};
    Declarator* existing_declarator{};

    auto it = scope.declarator_map.find(identifier.text);
    if (it != scope.declarator_map.end()) {
        existing_declarator = it->second;
        if (!existing_declarator->type) {
            new_declarator = existing_declarator;
            assert(new_declarator->identifier.text == identifier.text);

            new_declarator->declaration = declaration;
            new_declarator->type = type;
            new_declarator->location = location;
        }
    }

    if (!new_declarator) {
        new_declarator = new Declarator(declaration, type, identifier, location);

        if (existing_declarator) {
            assert(!primary || existing_declarator->primary == primary);

            new_declarator->primary = existing_declarator->primary;
            new_declarator->next = existing_declarator->next;
            existing_declarator->next = new_declarator;

        } else {
            existing_declarator = new_declarator;
            scope.declarator_map[identifier.text] = new_declarator;

            if (primary) {
                new_declarator->primary = primary;
            }
        }
    }

    if (new_declarator->type) {
        scope.declarators.push_back(new_declarator);
    }

    return new_declarator;
}

void IdentifierMap::push_scope(Scope&& scope) {
    scopes.push_front(move(scope));
}

Scope IdentifierMap::pop_scope() {
    auto scope = move(scopes.front());
    scopes.pop_front();
    return scope;
}

IdentifierMap::IdentifierMap(bool preparse): preparse(preparse) {
    push_scope();
}
