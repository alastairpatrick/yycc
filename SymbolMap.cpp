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

bool SymbolMap::is_type_identifier(const string* identifier) const {
    auto decl = lookup_decl(TypeNameKind::ORDINARY, identifier);
    if (!decl) return false;

    return decl->is_type();
}

void SymbolMap::add_decl(const string* name, const Decl* decl) {
    scopes.front().declarations[name] = decl;
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
