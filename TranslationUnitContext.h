#ifndef COMPILE_CONTEXT_H
#define COMPILE_CONTEXT_H

#include "parser/TypeContext.h"
#include "InternedString.h"

struct ASTNode;

enum class Severity;

struct TranslationUnitContext {
    static thread_local TranslationUnitContext* it;

    explicit TranslationUnitContext(ostream& message_stream);
    ~TranslationUnitContext();
    void operator=(const TranslationUnitContext&) = delete;

    ostream& message_stream;
    Severity highest_severity{};

    TypeContext type;

    list<string> interned_strings; // TODO: bump allocator
    unordered_set<string_view> interned_views;

    ASTNode* ast_nodes{};
};

#endif
