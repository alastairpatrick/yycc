#include "Statement.h"
#include "Declaration.h"
#include "visit/Visitor.h"

Statement::Statement(const Location& location): location(location) {
}

void Statement::print(ostream& stream) const {
    if (labels.empty()) return;

    for (auto& label: labels) {
        switch (label.kind) {
          case LabelKind::GOTO:
            stream << "[\"label\", \"" << label.identifier << "\"], ";
            break;
          case LabelKind::CASE:
            stream << "[\"case\", " << label.case_expr << "], ";
            break;
          case LabelKind::DEFAULT:
            stream << "[\"default\"], ";
            break;
        }
    }
}

CompoundStatement::CompoundStatement(Scope&& scope, ASTNodeVector&& nodes, const Location& location)
    : Statement(location), scope(scope), nodes(move(nodes)) {
}

VisitStatementOutput CompoundStatement::accept(Visitor& visitor, const VisitStatementInput& input) {
    return visitor.visit(this, input);
}

void CompoundStatement::print(ostream& stream) const {
    stream << '[';
    Statement::print(stream);
    stream << "\"block\", " << nodes << ']';
}



ForStatement::ForStatement(Declaration* declaration,  Expr* initialize, Expr* condition, Expr* iterate, Statement* body, const Location& location)
    : Statement(location), declaration(declaration), initialize(initialize), condition(condition), iterate(iterate), body(body) {
}

VisitStatementOutput ForStatement::accept(Visitor& visitor, const VisitStatementInput& input) {
    return visitor.visit(this, input);
}

void ForStatement::print(ostream& stream) const {
    stream << '[';
    Statement::print(stream);
    stream << "\"for\", " << declaration << ", " << initialize << ", " << condition << ", " << iterate << ", " << body << ']';
}


GoToStatement::GoToStatement(const Identifier& identifier, const Location& location)
    : Statement(location), identifier(identifier) {
}

VisitStatementOutput GoToStatement::accept(Visitor& visitor, const VisitStatementInput& input) {
    return visitor.visit(this, input);
}

void GoToStatement::print(ostream& stream) const {
    stream << "[\"goto\", \"" << identifier << "\"]";
}


IfElseStatement::IfElseStatement(Expr* condition, Statement* then_statement, Statement* else_statement, const Location& location)
    : Statement(location), condition(condition), then_statement(then_statement), else_statement(else_statement) {
}

VisitStatementOutput IfElseStatement::accept(Visitor& visitor, const VisitStatementInput& input) {
    return visitor.visit(this, input);
}

void IfElseStatement::print(ostream& stream) const {
    stream << '[';
    Statement::print(stream);
    stream << "\"if\", " << condition << ", " << then_statement << ", " << else_statement << ']';
}


ReturnStatement::ReturnStatement(Expr* expr, const Location& location)
    : Statement(location), expr(expr) {
}

VisitStatementOutput ReturnStatement::accept(Visitor& visitor, const VisitStatementInput& input) {
    return visitor.visit(this, input);
}

void ReturnStatement::print(ostream& stream) const {
    stream << '[';
    Statement::print(stream);

    stream << "\"return\"";
    if (expr) {
        stream << ", " << expr;
    }
    stream << ']';
}
