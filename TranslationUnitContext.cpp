#include "TranslationUnitContext.h"

#include "Message.h"
#include "parser/ASTNode.h"

thread_local TranslationUnitContext* TranslationUnitContext::it;

TranslationUnitContext::TranslationUnitContext(ostream& message_stream): message_stream(message_stream) {
    assert(!it);
    it = this;

    null_message_stream.setstate(ios_base::badbit);

    interned_views.insert(*empty_interned_string);

    type_emit_context.outcome = EmitOutcome::TYPE;

    fold_emit_context.outcome = EmitOutcome::FOLD;
    fold_emit_context.builder = LLVMCreateBuilder();
}

TranslationUnitContext::~TranslationUnitContext() {
    while (ast_nodes) {
        auto node = ast_nodes;
        ast_nodes = ast_nodes->next_delete;
        delete node;
    }

    assert(it == this);
    it = nullptr;
}
