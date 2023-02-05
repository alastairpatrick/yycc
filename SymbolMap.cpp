#include "SymbolMap.h"
#include "Decl.h"

const Decl* SymbolMap::lookup_decl(TypeNameKind kind, const string* name) const {
    // TODO: consider kind
    for (auto& scope : scopes) {
        auto it = scope.declarations.find(name);
        if (it != scope.declarations.end()) {
            return it->second;
        }
    }
    return nullptr;
}

const Type* SymbolMap::lookup_type(TypeNameKind kind, const string* name) const {
    auto decl = lookup_decl(kind, name);
    if (!decl) return nullptr;

    return decl->to_type();
}

void SymbolMap::add_decl(TypeNameKind kind, const string* name, const Decl* decl) {
    auto it = scopes.front().declarations.find(name);
    if (it != scopes.front().declarations.end()) {
        it->second->redeclare(decl);
    } else {
        scopes.front().declarations[name] = decl;
    }
}

void SymbolMap::push_scope() {
    scopes.push_front(Scope());
}

void SymbolMap::pop_scope() {
    scopes.pop_front();
}

SymbolMap::SymbolMap() {
    push_scope();
}
