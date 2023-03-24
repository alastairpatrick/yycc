#include "Context.h"

#include "parser/ASTNode.h"
#include "Message.h"

thread_local Context* Context::it;

Context::Context(ostream& message_stream): message_stream(message_stream) {
    assert(!it);
    it = this;

    interned_views.insert(*empty_interned_string);
}

Context::~Context() {
    while (ast_nodes) {
        auto node = ast_nodes;
        ast_nodes = ast_nodes->next_delete;
        delete node;
    }

    assert(it == this);
    it = nullptr;
}
