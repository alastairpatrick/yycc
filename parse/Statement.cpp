#include "Statement.h"
#include "Declaration.h"
#include "visit/Visitor.h"

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



ForStatement::ForStatement(Declaration* declaration,  Expr* initialize, Expr* condition, Expr* iterate, Statement* body, const Location& location)
    : Statement(location), declaration(declaration), initialize(initialize), condition(condition), iterate(iterate), body(body) {
}

VisitStatementOutput ForStatement::accept(Visitor& visitor, const VisitStatementInput& input) {
    return visitor.visit(this, input);
}

void ForStatement::print(ostream& stream) const {
    stream << "[\"for\", " << declaration << ", " << initialize << ", " << condition << ", " << iterate << ", " << body << ']';
}



ReturnStatement::ReturnStatement(Expr* expr, const Location& location)
    : Statement(location), expr(expr) {
}

VisitStatementOutput ReturnStatement::accept(Visitor& visitor, const VisitStatementInput& input) {
    return visitor.visit(this, input);
}

void ReturnStatement::print(ostream& stream) const {
    stream << "[\"return\"";
    if (expr) {
        stream << ", " << expr;
    }
    stream << ']';
}
