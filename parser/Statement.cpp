#include "Statement.h"
#include "visitor/Visitor.h"

Statement::Statement(const Location& location): location(location) {
}

CompoundStatement::CompoundStatement(ASTNodeVector&& items, const Location& location)
    : Statement(location), items(move(items)) {
}

VisitStatementOutput CompoundStatement::accept(Visitor& visitor, const VisitStatementInput& input) {
    return visitor.visit(this, input);
}

void CompoundStatement::print(ostream& stream) const {
    stream << "[\"block\", " << items << ']';
}

ReturnStatement::ReturnStatement(Expr* value, const Location& location)
    : Statement(location), value(value) {
}

VisitStatementOutput ReturnStatement::accept(Visitor& visitor, const VisitStatementInput& input) {
    return visitor.visit(this, input);
}

void ReturnStatement::print(ostream& stream) const {
    stream << "[\"return\", " << value << ']';
}
