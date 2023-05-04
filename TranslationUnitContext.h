#ifndef TRANSLATION_UNIT_CONTEXT_H
#define TRANSLATION_UNIT_CONTEXT_H

#include "InternedString.h"
#include "parse/TypeContext.h"

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
    unordered_set<pair<string_view, string_view>, InternedStringPairHash, InternedStringPairKeyEqual> interned_views;

    ASTNode* ast_nodes{};

    LLVMContextRef llvm_context{};
    LLVMTargetDataRef llvm_target_data{};
    LLVMValueRef zero_size{};
    LLVMValueRef zero_int{};
};

#endif
