#ifndef CTX_H
#define CTX_H

#include "std.h"
#include "type.h"

// The state shared between the lexer and parser that makes the grammar not context free.
struct ParseContext {
    struct Scope {
        unordered_set<string> types;
    };

    ParseContext() {
        push_scope();
    }

    void set_is_type(const string& identifier) {
        scopes.front().types.insert(identifier);
    }

    bool is_type(const string& identifier) const {
        for (auto& scope : scopes) {
            if (scope.types.find(identifier) != scope.types.end()) return true;
        }
        return false;
    }

    void push_scope() {
        scopes.emplace_front(Scope());
    }

    void pop_scope() {
        scopes.pop_front();
    }

    list<Scope> scopes;
};

#endif