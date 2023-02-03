#include "std.h"
#include "stmt.h"


CompoundStatement::CompoundStatement(DeclStatementList items, const Location& location)
    : Statement(location), items(move(items)) {
}

void CompoundStatement::print(std::ostream& stream) const {
    stream << "[\"block\", " << items << ']';
}

ReturnStatement::ReturnStatement(shared_ptr<Expr> value, const Location& location)
    : Statement(location), value(value) {
}

void ReturnStatement::print(std::ostream& stream) const {
    stream << "[\"return\", " << value << ']';
}
