#include "IdentifierMap.h"
#include "Declaration.h"
#include "Message.h"

Declarator* IdentifierMap::lookup_declarator(const Identifier& identifier) const {
    for (auto& scope : scopes) {
        auto it = scope.declarator_map.find(identifier.name);
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

Declarator* IdentifierMap::add_declarator(AddDeclaratorScope add_scope, const Declaration* declaration, const Type* type, const Identifier& identifier, const Location& location) {
    Declarator* declarator{};
    if (add_scope != AddDeclaratorScope::CURRENT) {
        declarator = find_placeholder(scopes.back(), declaration, type, identifier, location);
        if (declarator) return declarator;
    }

    if (add_scope != AddDeclaratorScope::FILE) {
        declarator = find_placeholder(scopes.front(), declaration, type, identifier, location);
        if (declarator) return declarator;
    }

    declarator = new Declarator(declaration, type, identifier, location);

    if (identifier.name->empty()) return declarator;

    Declarator* primary{};
    if (add_scope != AddDeclaratorScope::CURRENT) {
        primary = add_declarator_to_scope(scopes.back(), declarator);
    }

    if (add_scope != AddDeclaratorScope::FILE) {
        auto primary2 = add_declarator_to_scope(scopes.front(), declarator);
        if (primary2) {
            assert(primary == nullptr || primary == primary2);
            primary = primary2;
        }
    }

    assert(declarator->primary == declarator);

    // Maintain a singly linked list of declarators linked to the same identifier, with the first such declarator
    // encountered - the primary - always being the first node in the list.
    if (primary) {
        declarator->primary = primary;
        declarator->next = primary->next;
        primary->next = declarator;
    }

    return declarator;
}

Declarator* IdentifierMap::find_placeholder(Scope& scope, const Declaration* declaration, const Type* type, const Identifier& identifier, const Location& location) {
    if (!identifier.name->empty()) {
        auto it = scopes.back().declarator_map.find(identifier.name);
        if (it != scopes.back().declarator_map.end()) {
            auto declarator = it->second;
            if (!declarator->delegate) {
                assert(declarator->identifier.name == identifier.name);
                declarator->declaration = declaration;
                declarator->type = type;
                declarator->location = location;
                return declarator;
            }
        }
    }
    return nullptr;
}

Declarator* IdentifierMap::add_declarator_to_scope(Scope& scope, Declarator* declarator) {
    auto it = scope.declarator_map.find(declarator->identifier.name);
    if (it == scope.declarator_map.end()) {
        scope.declarator_map[declarator->identifier.name] = declarator;
        //scope.declarators.push_back(declarator);
        return nullptr;
    }

    return it->second->primary;
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
