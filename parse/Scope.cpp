#include "Scope.h"

Scope::Scope(ScopeKind kind): kind(kind) {}

Scope::Scope(ScopeKind kind, string_view identifier)
    : kind(kind), prefix(intern_string(identifier, "::")) {
}

Declarator* Scope::lookup_declarator(const Identifier& identifier) const {
    for (auto scope = this; scope; scope = scope->parent) {
        InternedString identifier_string = scope->parent ? identifier.text : identifier.usage_at_file_scope;

        auto it = scope->declarator_map.find(identifier_string);
        if (it != scope->declarator_map.end()) {
            // Note that this intentionally does _not_ always return the primary. For reporting errors
            // it is better to return a declarator that is currently in scope. The primary declarator
            // might not be in scope, e.g. it has extern storage class and block scope.
            return it->second;
        }
    }

    return nullptr;
}

Declarator* Scope::lookup_member(InternedString identifier) const {
    auto it = declarator_map.find(identifier);
    if (it == declarator_map.end()) return nullptr;

    return it->second;
}
