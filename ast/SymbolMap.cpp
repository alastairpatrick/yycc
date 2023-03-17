#include "SymbolMap.h"
#include "CompileContext.h"
#include "Declaration.h"

Declarator* SymbolMap::lookup_declarator(bool tag, const Identifier& identifier) const {
    // TODO: consider tag
    for (auto& scope : scopes) {
        auto it = scope.declarators.find(identifier.name);
        if (it != scope.declarators.end()) {
            return it->second;
        }
    }

    return nullptr;
}

const Type* SymbolMap::lookup_type(bool tag, const Identifier& identifier) const {
    auto declarator = lookup_declarator(tag, identifier);
    if (!declarator) return nullptr;

    return declarator->to_type();
}

void SymbolMap::add_declarator(TypeNameKind kind, Declarator* declarator) {
    auto it = scopes.front().declarators.find(declarator->identifier.name);
    if (it != scopes.front().declarators.end()) {
        declarator->earlier = it->second;
        if (!preparse) declarator->combine();
        it->second = declarator;
    } else {
        scopes.front().declarators[declarator->identifier.name] = declarator;
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
