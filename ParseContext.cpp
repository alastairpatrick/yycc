#include "ParseContext.h"

#include "Decl.h"

ParseContext::ParseContext() {
    push_scope();
}

void ParseContext::set_is_type(const Decl* decl) {
    scopes.front().types.insert(&decl->identifier);
}

bool ParseContext::is_type(const string& identifier) const {
    for (auto& scope : scopes) {
        if (scope.types.find(&identifier) != scope.types.end()) return true;
    }
    return false;
}

void ParseContext::push_scope() {
    scopes.emplace_front(Scope());
}

void ParseContext::pop_scope() {
    scopes.pop_front();
}
