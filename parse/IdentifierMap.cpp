#include "IdentifierMap.h"
#include "Declaration.h"
#include "Message.h"

Declarator* IdentifierMap::lookup_declarator(const Identifier& identifier) const {
    return scopes.front()->lookup_declarator(identifier);

}

Declarator* IdentifierMap::add_declarator(AddScope add_scope,
                                          const Declaration* declaration,
                                          const Type* type,
                                          const Identifier& identifier,
                                          DeclaratorDelegate* delegate,
                                          const Location& location,
                                          Declarator* primary) {
    if (identifier.text->empty()) return new Declarator(declaration, type, identifier.text, delegate, location);

    if (add_scope == AddScope::FILE) {
        return add_declarator_internal(scopes.back(), declaration, type, identifier, delegate, location, primary);
    } else if (add_scope == AddScope::TOP) {
        return add_declarator_internal(scopes.front(), declaration, type, identifier, delegate, location, primary);
    } else {
        auto file_or_block_scope = scopes.front();
        for (; file_or_block_scope && file_or_block_scope->kind == ScopeKind::STRUCTURED; file_or_block_scope = file_or_block_scope->parent);

        Declarator* declarator = add_declarator_internal(file_or_block_scope, declaration, type, identifier, delegate, location, primary);
        primary = declarator->primary;

        if (add_scope == AddScope::FILE_OR_BLOCK_AND_TOP) {
            declarator = add_declarator_internal(scopes.front(), declaration, type, identifier, delegate, location, primary);
        }

        return declarator;
    }
}

Declarator* IdentifierMap::add_declarator_internal(Scope* scope,
                                                   const Declaration* declaration,
                                                   const Type* type,
                                                   const Identifier& identifier,
                                                   DeclaratorDelegate* delegate,
                                                   const Location& location,
                                                   Declarator* primary) {
    Declarator* new_declarator{};
    Declarator* existing_declarator{};

    InternedString identifier_for_scope;
    if (scope->kind == ScopeKind::BLOCK || scope->kind == ScopeKind::PROTOTYPE) {
        identifier_for_scope = identifier.text;
        auto colon_pos = identifier_for_scope->rfind(':');
        if (colon_pos != identifier_for_scope->npos) {
            identifier_for_scope = intern_string(identifier_for_scope->substr(colon_pos + 1));
            message(Severity::ERROR, location) << "identifiers at this scope may not be qualified; consider using '" << *identifier_for_scope << "'\n";
        }
    } else {
        identifier_for_scope = identifier.qualified;
    }

    auto it = scope->declarator_map.find(identifier_for_scope);
    if (it != scope->declarator_map.end()) {
        existing_declarator = it->second;
        if (!existing_declarator->type) {
            new_declarator = existing_declarator;
            assert(new_declarator->identifier == identifier_for_scope);

            new_declarator->declaration = declaration;
            new_declarator->type = type;
            new_declarator->delegate = delegate;
            new_declarator->location = location;
        }
    }

    if (!new_declarator) {
        new_declarator = new Declarator(declaration, type, identifier_for_scope, delegate, location);

        if (existing_declarator) {
            assert(!primary || existing_declarator->primary == primary);

            new_declarator->primary = existing_declarator->primary;
            new_declarator->next = existing_declarator->next;
            existing_declarator->next = new_declarator;

        } else {
            existing_declarator = new_declarator;
            scope->declarator_map[identifier_for_scope] = new_declarator;

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

void IdentifierMap::push_scope(Scope* scope) {
    if (scopes.size()) {
        scope->parent = scopes.front();
    }
    scopes.push_front(scope);
}

Scope* IdentifierMap::pop_scope() {
    auto popped = scopes.front();
    scopes.pop_front();
    return popped;
}

IdentifierMap::IdentifierMap() {
    push_scope(new Scope(ScopeKind::FILE));
}

ScopeKind IdentifierMap::scope_kind() const {
    return scopes.front()->kind;
}
