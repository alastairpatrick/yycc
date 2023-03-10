#include "ASTNode.h"

#include "CompileContext.h"

ASTNode::ASTNode(const Location& location): location(location) {
    auto context = CompileContext::it;
    next_delete = context->ast_nodes;
    context->ast_nodes = this;
}

ostream& operator<<(ostream& stream, const ASTNodeVector& items) {
    stream << '[';
    auto separate = false;
    for (auto item : items) {
        if (separate) stream << ", ";
        separate = true;
        stream << item;
    }
    return stream << ']';
}
