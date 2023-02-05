#include "ASTNode.h"

#include "CompileContext.h"

ASTNode::ASTNode(const Location& location): location(location) {
    auto context = CompileContext::it;
    next_delete = context->ast_nodes;
    context->ast_nodes = this;
}

ostream& operator<<(ostream& stream, const ASTNodeVector& items) {
    stream << '[';
    for (auto i = 0; i < items.size(); ++i) {
        if (i != 0) stream << ", ";
        stream << items[i];
    }
    return stream << ']';
}
