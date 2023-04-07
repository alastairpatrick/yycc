#include "TranslationUnitContext.h"

#include "parser/ASTNode.h"
#include "Message.h"

thread_local TranslationUnitContext* TranslationUnitContext::it;

TranslationUnitContext::TranslationUnitContext(ostream& message_stream): message_stream(message_stream) {
    assert(!it);
    it = this;

    null_message_stream.setstate(ios_base::badbit);

    interned_views.insert(*empty_interned_string);
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
