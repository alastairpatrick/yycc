#include "IdentifierMap.h"
#include "Declaration.h"

Declarator* IdentifierMap::lookup_declarator(const Identifier& identifier) const {
    for (auto& scope : scopes) {
        auto it = scope.declarators.find(identifier.name);
        if (it != scope.declarators.end()) {
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

bool IdentifierMap::add_declarator(Declarator* declarator) {
    bool result = true;
    auto declaration = declarator->declaration;

    if (declaration->scope != IdentifierScope::FILE) {
        result = add_declarator_to_scope(scopes.front(), declarator) && result;
    }

    if (declaration->scope == IdentifierScope::FILE || declaration->storage_class == StorageClass::EXTERN) {
        result = add_declarator_to_scope(scopes.back(), declarator) && result;
    }

    return result;
}

bool IdentifierMap::add_declarator_to_scope(Scope& scope, Declarator* declarator) {
    auto it = scope.declarators.find(declarator->identifier.name);
    if (it != scope.declarators.end()) {
        if (preparse) {
            declarator->earlier = it->second;
        } else {
            it->second->compose(declarator);
            declarator = it->second;
            return false;
        }
    } else {
        scope.declarators[declarator->identifier.name] = declarator;
    }

    return true;
}

void IdentifierMap::push_scope() {
    scopes.push_front(Scope());
}

void IdentifierMap::pop_scope() {
    scopes.pop_front();
}

IdentifierMap::IdentifierMap(bool preparse): preparse(preparse) {
    push_scope();
}
