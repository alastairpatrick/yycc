#include "Statement.h"

Statement::Statement(const Location& location): location(location) {
}

void Statement::resolve(ResolutionContext& context) {
}

CompoundStatement::CompoundStatement(ASTNodeVector&& items, const Location& location)
    : Statement(location), items(move(items)) {
}

void CompoundStatement::print(ostream& stream) const {
    stream << "[\"block\", " << items << ']';
}

ReturnStatement::ReturnStatement(Expr* value, const Location& location)
    : Statement(location), value(value) {
}

void ReturnStatement::print(ostream& stream) const {
    stream << "[\"return\", " << value << ']';
}
