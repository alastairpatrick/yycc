#ifndef COMPILE_CONTEXT_H
#define COMPILE_CONTEXT_H

#include "EmitContext.h"
#include "parser/TypeContext.h"
#include "InternedString.h"

struct ASTNode;
struct Printable;
enum class Severity;

struct TranslationUnitContext {
    static thread_local TranslationUnitContext* it;

    explicit TranslationUnitContext(ostream& message_stream);
    ~TranslationUnitContext();
    void operator=(const TranslationUnitContext&) = delete;

    ostream& message_stream;
    stringstream null_message_stream;
    bool messages_active = true;
    Severity highest_severity{};
    unordered_set<const Printable*> printing;

    TypeContext type;

    list<string> interned_strings; // TODO: bump allocator
    unordered_set<string_view> interned_views;

    ASTNode* ast_nodes{};

    EmitContext type_emit_context;
    EmitContext fold_emit_context;
};

#endif
