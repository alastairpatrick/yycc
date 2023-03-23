#ifndef COMPILE_CONTEXT_H
#define COMPILE_CONTEXT_H

#include "parser/TypeContext.h"
#include "InternedString.h"

struct ASTNode;

struct CompileContext {
    static thread_local CompileContext* it;

    explicit CompileContext(ostream& message_stream);
    ~CompileContext();
    void operator=(const CompileContext&) = delete;

    ostream& message_stream;

    TypeContext type;

    list<string> interned_strings; // TODO: bump allocator
    unordered_set<string_view> interned_views;

    ASTNode* ast_nodes{};
};

#endif
