#include "Scope.h"

Scope(ScopeKind kind): kind(kind) {}

Scope(ScopeKind kind, string_view identifier)
    : kind(kind), prefix(intern_string(identifier, "::")) {
}

Declarator* lookup_declarator(const Identifier& identifier) const