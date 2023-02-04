#ifndef CTX_H
#define CTX_H

#include "std.h"
#include "Type.h"

struct Decl;

// The state shared between the lexer and parser that makes the grammar not context free.
struct ParseContext {
    struct StringPtrEq {
        bool operator()(const string* a, const string* b) const {
          return *a == *b;
        }
    };

    struct StringPtrHash {
        size_t operator()(const string* a) const {
            return hash<string>()(*a);
        }
    };

    struct Scope {
        // The key points to the identifier string of the declaration AST node.
        unordered_set<const string*, StringPtrHash, StringPtrEq> types;
    };

    ParseContext();

    void set_is_type(const Decl* decl);
    bool is_type(const string& identifier) const;

    void push_scope();
    void pop_scope();

    list<Scope> scopes;
};

#endif