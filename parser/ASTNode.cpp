#include "ASTNode.h"

#include "TranslationUnitContext.h"

ASTNode::ASTNode() {
    auto context = TranslationUnitContext::it;
    next_delete = context->ast_nodes;
    context->ast_nodes = this;
}

ASTNode::ASTNode(const ASTNode&): ASTNode() {
}

ASTNode::ASTNode(ASTNode&&): ASTNode() {
}

ASTNode& ASTNode::operator=(const ASTNode&) {
    return *this;
}

ASTNode& ASTNode::operator=(ASTNode&&) {
    return *this;
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
