#ifndef COMPILE_CONTEXT_H
#define COMPILE_CONTEXT_H

#include "lexer/LexerContext.h"
#include "parser/TypeContext.h"
#include "InternedString.h"

struct ASTNode;

struct Context {
    static thread_local Context* it;

    explicit Context(ostream& message_stream);
    ~Context();
    void operator=(const Context&) = delete;

    ostream& message_stream;

    TypeContext type;
    LexerContext lexer;

    list<string> interned_strings; // TODO: bump allocator
    unordered_set<string_view> interned_views;

    ASTNode* ast_nodes{};
};

#endif
