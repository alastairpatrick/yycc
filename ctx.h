#ifndef CTX_H
#define CTX_H

#include "std.h"
#include "type.h"

// The state shared between the lexer and parser that makes the grammar not context free.
struct ParseContext {
    void set_is_type(const string& identifier) {
        types.insert(identifier);
    }

    bool is_type(const string& identifier) const {
        return types.find(identifier) != types.end();
    }

private:
    unordered_set<string> types;
};

#endif