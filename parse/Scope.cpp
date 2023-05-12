#include "Scope.h"

Scope::Scope(ScopeKind kind): kind(kind) {}

Scope::Scope(ScopeKind kind, InternedString namespace_prefix)
    : kind(kind), namespace_prefix(namespace_prefix) {
}

Declarator* Scope::lookup_declarator(const Identifier& identifier) const {
    for (auto scope = this; scope; scope = scope->parent) {
        InternedString identifier_string = scope->parent ? identifier.text : identifier.usage_at_file_scope;

        auto it = scope->declarator_map.find(identifier_string);
        if (it == scope->declarator_map.end()) continue;

        if (it->second->identifier_position >= identifier.position) continue;

        return it->second;
    }

    return nullptr;
}

Declarator* Scope::lookup_member(const Identifier& identifier) const {
    auto it = declarator_map.find(identifier.qualified);
    if (it != declarator_map.end()) return it->second;

    if (identifier.text->find(':') != identifier.text->npos) return nullptr;

    InternedString requalified = intern_string(*namespace_prefix, *identifier.text);
    it = declarator_map.find(requalified);
    if (it != declarator_map.end()) return it->second;

    return nullptr;
}
