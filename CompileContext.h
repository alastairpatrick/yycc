#ifndef COMPILE_CONTEXT_H
#define COMPILE_CONTEXT_H

#include "TypeContext.h"

struct CompileContext {
    static thread_local CompileContext* it;

    CompileContext();
    ~CompileContext();

    TypeContext type;

    unordered_set<string> interned_strings;
};

// Interned strings have the property that it their string values are equal, their addresses are the same also.
// This means they have be used as keys in sets and maps with a very fast equality test. Note that they might be
// a poor fit for ordered set and maps because the order will be their address order rather than their
// lexicographic order.
const string* intern(string source);

#endif
