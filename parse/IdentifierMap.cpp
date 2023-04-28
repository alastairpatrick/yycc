#include "IdentifierMap.h"
#include "Declaration.h"
#include "Message.h"

Declarator* IdentifierMap::lookup_declarator(const Identifier& identifier) const {
    for (auto& scope : scopes) {
        auto it = scope.declarators.find(identifier.name);
        if (it != scope.declarators.end()) {
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

Declarator* IdentifierMap::add_declarator(const Declaration* declaration, const Type* type, const Identifier& identifier, const Location& location) {
    Declarator* declarator{};
    if (declaration->scope == IdentifierScope::FILE || declaration->storage_class == StorageClass::EXTERN) {
        declarator = find_placeholder(scopes.back(), declaration, type, identifier, location);
        if (declarator) return declarator;
    }

    if (declaration->scope != IdentifierScope::FILE) {
        declarator = find_placeholder(scopes.front(), declaration, type, identifier, location);
        if (declarator) return declarator;
    }

    declarator = new Declarator(declaration, type, identifier, location);

    if (identifier.name->empty()) return declarator;

    Declarator* primary{};
    if (declaration->scope == IdentifierScope::FILE || declaration->storage_class == StorageClass::EXTERN) {
        primary = add_declarator_to_scope(scopes.back(), declarator);
    }

    if (declaration->scope != IdentifierScope::FILE) {
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
    } else {
        primary_declarators.insert(declarator);
    }

    return declarator;
}

Declarator* IdentifierMap::find_placeholder(Scope& scope, const Declaration* declaration, const Type* type, const Identifier& identifier, const Location& location) {
    if (!identifier.name->empty()) {
        auto it = scopes.back().declarators.find(identifier.name);
        if (it != scopes.back().declarators.end()) {
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
    auto it = scope.declarators.find(declarator->identifier.name);
    if (it == scope.declarators.end()) {
        scope.declarators[declarator->identifier.name] = declarator;
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
