#ifndef COMPILE_CONTEXT_H
#define COMPILE_CONTEXT_H

#include "TypeContext.h"
#include "Location.h"

struct ASTNode;

struct CompileContext {
    static thread_local CompileContext* it;

    CompileContext(ostream& message_stream);
    ~CompileContext();

    ostream& message_stream;

    TypeContext type;

    const string* empty_string;
    unordered_set<string> interned_strings;

    ASTNode* ast_nodes{};
};


ostream& message(const Location& location);

// Interned strings have the property that it their string values are equal, their addresses are the same also.
// This means they can be used as keys in sets and maps with a very fast equality test. Note that they might be
// a poor fit for ordered set and maps because the order is their address order rather than their
// lexicographic order.
const string* intern(string source);
const string* empty_string();

#endif
