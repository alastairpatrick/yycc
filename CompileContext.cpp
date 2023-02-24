#include "CompileContext.h"

#include "std.h"

#include "ASTNode.h"

thread_local CompileContext* CompileContext::it;

CompileContext::CompileContext(ostream& message_stream): message_stream(message_stream) {
    assert(!it);
    it = this;

    interned_views.insert(*EmptyInternedString);
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

ostream& message(Severity severity, const Location& location) {
    auto &stream = CompileContext::it->message_stream;
    stream << location.file << ':' << location.line << ':' << location.column << ": ";

    switch (severity) {
    case Severity::WARNING:
        stream << "warning ";
        break;
    case Severity::ERROR:
        stream << "error ";
        break;
    }

    return stream;
}
