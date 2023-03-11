#include "CompileContext.h"

#include "ASTNode.h"
#include "Message.h"

thread_local CompileContext* CompileContext::it;

CompileContext::CompileContext(ostream& message_stream): message_stream(message_stream) {
    assert(!it);
    it = this;

    interned_views.insert(*empty_interned_string);
}

CompileContext::~CompileContext() {
    while (ast_nodes) {
        auto node = ast_nodes;
        ast_nodes = ast_nodes->next_delete;
        delete node;
    }

    assert(it == this);
    it = nullptr;
}
