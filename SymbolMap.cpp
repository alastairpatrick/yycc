#include "SymbolMap.h"
#include "CompileContext.h"
#include "Declaration.h"

Declarator* SymbolMap::lookup_declarator(TypeNameKind kind, const Identifier& identifier) {
    // TODO: consider kind
    for (auto& scope : scopes) {
        auto it = scope.declarators.find(identifier.name);
        if (it != scope.declarators.end()) {
            auto declarator = it->second;

            while (declarator && declarator->identifier.byte_offset > identifier.byte_offset) {
                declarator = declarator->earlier;
            }

            return declarator;
        }
    }

    return nullptr;
}

const Type* SymbolMap::lookup_type(TypeNameKind kind, const Identifier& identifier) {
    auto declarator = lookup_declarator(kind, identifier);
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
