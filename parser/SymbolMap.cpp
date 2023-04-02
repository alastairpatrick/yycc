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

void SymbolMap::add_root_declarator(Declarator* declarator) {
    add_declarator_internal(scopes.back(), declarator);
}

void SymbolMap::add_declarator(Declarator* declarator) {
    add_declarator_internal(scopes.front(), declarator);
}

void SymbolMap::add_declarator_internal(Scope& scope, Declarator* declarator) {
    auto it = scope.declarators.find(declarator->identifier.name);
    if (it != scope.declarators.end()) {
        declarator->earlier = it->second;
        if (!preparse) declarator->combine();
        it->second = declarator;
    } else {
        scope.declarators[declarator->identifier.name] = declarator;
    }
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
