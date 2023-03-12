#include "SymbolMap.h"
#include "CompileContext.h"
#include "Decl.h"

Decl* SymbolMap::lookup_decl(TypeNameKind kind, const Identifier& identifier) {
    // TODO: consider kind
    for (auto& scope : scopes) {
        auto it = scope.declarations.find(identifier.name);
        if (it != scope.declarations.end()) {
            auto decl = it->second;

            while (decl && decl->identifier.byte_offset > identifier.byte_offset) {
                decl = decl->earlier;
            }

            return decl;
        }
    }

    return nullptr;
}

const Type* SymbolMap::lookup_type(TypeNameKind kind, const Identifier& identifier) {
    auto decl = lookup_decl(kind, identifier);
    if (!decl) return nullptr;

    return decl->to_type();
}

void SymbolMap::add_decl(TypeNameKind kind, Decl* decl) {
    auto it = scopes.front().declarations.find(decl->identifier.name);
    if (it != scopes.front().declarations.end()) {
        decl->earlier = it->second;
        if (!preparse) decl->combine();
        it->second = decl;
    } else {
        scopes.front().declarations[decl->identifier.name] = decl;
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
