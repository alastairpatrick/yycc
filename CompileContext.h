#ifndef COMPILE_CONTEXT_H
#define COMPILE_CONTEXT_H

#include "TypeContext.h"
#include "InternedString.h"
#include "Location.h"

struct ASTNode;

enum class Severity {
    INFO,
    WARNING,
    ERROR,
};

struct CompileContext {
    static thread_local CompileContext* it;

    CompileContext(ostream& message_stream);
    ~CompileContext();

    ostream& message_stream;

    TypeContext type;

    list<string> interned_strings; // TODO: bump allocator
    unordered_set<string_view> interned_views;

    ASTNode* ast_nodes{};
};


ostream& message(Severity severity, const Location& location);

#endif
