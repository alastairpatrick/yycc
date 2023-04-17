#include "Statement.h"
#include "visitor/Visitor.h"

Statement::Statement(const Location& location): location(location) {
}

CompoundStatement::CompoundStatement(Scope&& scope, ASTNodeVector&& nodes, const Location& location)
    : Statement(location), scope(scope), nodes(move(nodes)) {
}

VisitStatementOutput CompoundStatement::accept(Visitor& visitor, const VisitStatementInput& input) {
    return visitor.visit(this, input);
}

void CompoundStatement::print(ostream& stream) const {
    stream << "[\"block\", " << nodes << ']';
}

ReturnStatement::ReturnStatement(Expr* expr, const Location& location)
    : Statement(location), expr(expr) {
}

VisitStatementOutput ReturnStatement::accept(Visitor& visitor, const VisitStatementInput& input) {
    return visitor.visit(this, input);
}

void ReturnStatement::print(ostream& stream) const {
    stream << "[\"return\", " << expr << ']';
}
