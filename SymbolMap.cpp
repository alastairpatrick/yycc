#include "SymbolMap.h"
#include "CompileContext.h"
#include "Decl.h"

Decl* SymbolMap::lookup_decl(TypeNameKind kind, const string* name) {
    // TODO: consider kind
    for (auto& scope : scopes) {
        auto it = scope.declarations.find(name);
        if (it != scope.declarations.end()) {
            auto decl = it->second;

            // Extern declarations with block scope are added to both their block scope and also to file scope.
            // Only return such declarations if found at block scope.
            if (decl->scope == IdentifierScope::FILE || &scope != &scopes.back()) return decl;
        }
    }

    auto it = mysteries.find(name);
    if (it != mysteries.end()) return it->second;

    return mysteries[name] = new Mystery(name);
}

const Type* SymbolMap::lookup_type(TypeNameKind kind, const string* name) {
    auto decl = lookup_decl(kind, name);
    if (!decl) return nullptr;

    return decl->to_type();
}

void SymbolMap::add_decl(TypeNameKind kind, const string* name, Decl* decl) {
    auto it = scopes.front().declarations.find(name);
    if (it != scopes.front().declarations.end()) {
        Decl** last_next{};
        for (auto existing_decl = it->second; existing_decl; existing_decl = existing_decl->scope_next) {
            last_next = &existing_decl->scope_next;

            existing_decl->parse_combine(decl);

            // int baz(void);
            // static int baz(void); // ERROR
            //if (decl->linkage == Linkage::INTERNAL && existing_decl->linkage != Linkage::INTERNAL) {
            //    message(decl->location) << "error static declaration of '" << decl->identifier << "' follows non-static\n";
            //}
        }

        *last_next = decl;
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
