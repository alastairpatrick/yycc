#include "SymbolMap.h"
#include "Declaration.h"

Declarator* SymbolMap::lookup_declarator(const Identifier& identifier) const {
    for (auto& scope : scopes) {
        auto it = scope.declarators.find(identifier.name);
        if (it != scope.declarators.end()) {
            return it->second;
        }
    }

    return nullptr;
}

const Type* SymbolMap::lookup_type(const Identifier& identifier) const {
    auto declarator = lookup_declarator(identifier);
    if (!declarator) return nullptr;

    return declarator->to_type();
}

bool SymbolMap::add_declarator(Declarator* declarator) {
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

bool SymbolMap::add_declarator_to_scope(Scope& scope, Declarator* declarator) {
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

void SymbolMap::push_scope() {
    scopes.push_front(Scope());
}

void SymbolMap::pop_scope() {
    scopes.pop_front();
}

SymbolMap::SymbolMap(bool preparse): preparse(preparse) {
    push_scope();
}

void SymbolMap::clear_internal_linkage() {
    auto& declarators = scopes.back().declarators;
    
    for (auto it = declarators.begin(); it != declarators.end();) {
        if (it->second->declaration->linkage() == Linkage::INTERNAL) {
            it = declarators.erase(it);
        } else {
            ++it;
        }
    }
}
