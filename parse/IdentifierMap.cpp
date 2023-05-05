#include "IdentifierMap.h"
#include "Declaration.h"
#include "Message.h"

Declarator* IdentifierMap::lookup_declarator(const Identifier& identifier) const {
    for (auto& scope : scopes) {
        InternedString identifier_string = &scope == &scopes.back() ? identifier.at_file_scope : identifier.text;

        auto it = scope.declarator_map.find(identifier_string);
        if (it != scope.declarator_map.end()) {
            // Note that this intentionally does _not_ always return the primary. For reporting errors
            // it is better to return a declarator that is currently in scope. The primary declarator
            // might not be in scope, e.g. it has extern storage class and block scope.
            return it->second;
        }
    }

    return nullptr;
}

Declarator* IdentifierMap::add_declarator(AddScope add_scope, const Declaration* declaration, const Type* type, const Identifier& identifier, const Location& location, Declarator* primary) {
    Scope* scope = add_scope == AddScope::FILE ? &scopes.back() : &scopes.front();

    InternedString identifier_string{};
    if (scope == &scopes.back()) {
        identifier_string = identifier.at_file_scope;
    } else {
        identifier_string = identifier.text;

        auto idx = identifier_string->rfind(':');
        if (idx != identifier_string->npos) {
            message(Severity::ERROR, location) << "qualified identifier not valid at this scope at file scope; did you mean '" << identifier_string->substr(idx + 1) << "'?\n";
        }
    }

    if (identifier_string->empty()) return new Declarator(declaration, type, identifier_string, location);

    Declarator* new_declarator{};
    Declarator* existing_declarator{};

    auto it = scope->declarator_map.find(identifier_string);
    if (it != scope->declarator_map.end()) {
        existing_declarator = it->second;
        if (!existing_declarator->type) {
            new_declarator = existing_declarator;
            assert(new_declarator->identifier == identifier_string);

            new_declarator->declaration = declaration;
            new_declarator->type = type;
            new_declarator->location = location;
        }
    }

    if (!new_declarator) {
        new_declarator = new Declarator(declaration, type, identifier_string, location);

        if (existing_declarator) {
            assert(!primary || existing_declarator->primary == primary);

            new_declarator->primary = existing_declarator->primary;
            new_declarator->next = existing_declarator->next;
            existing_declarator->next = new_declarator;

        } else {
            existing_declarator = new_declarator;
            scope->declarator_map[identifier_string] = new_declarator;

            if (primary) {
                new_declarator->primary = primary;
                new_declarator->next = primary->next;
                primary->next = new_declarator;
            }
        }
    }

    if (new_declarator->type) {
        scope->declarators.push_back(new_declarator);
    }

    return new_declarator;
}

void IdentifierMap::push_scope(ScopeKind kind) {
    scopes.push_front(Scope(kind));
}

void IdentifierMap::push_scope(Scope&& scope) {
    scopes.push_front(move(scope));
}

Scope IdentifierMap::pop_scope() {
    auto scope = move(scopes.front());
    scopes.pop_front();
    return scope;
}

IdentifierMap::IdentifierMap(bool preparse): preparse(preparse) {
    push_scope(Scope(ScopeKind::FILE));
}

ScopeKind IdentifierMap::scope_kind() const {
    return scopes.front().kind;
}
