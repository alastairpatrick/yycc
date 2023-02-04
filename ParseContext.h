#ifndef CTX_H
#define CTX_H

#include "std.h"
#include "Type.h"

struct Decl;

// The state shared between the lexer and parser that makes the grammar not context free.
struct ParseContext {
    struct Scope {
        unordered_set<const string*> types;
    };

    ParseContext();

    void set_is_type(const string* identifier);
    bool is_type(const string* identifier) const;

    void push_scope();
    void pop_scope();

    list<Scope> scopes;
};

#endif