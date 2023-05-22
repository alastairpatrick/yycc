#include "Statement.h"
#include "Declaration.h"
#include "pass/Visitor.h"

Statement::Statement(const Location& location): LocationNode(location) {
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

CompoundStatement::CompoundStatement(ASTNodeVector&& nodes, const Location& location)
    : Statement(location), nodes(move(nodes)) {
}

VisitStatementOutput CompoundStatement::accept(Visitor& visitor) {
    return visitor.visit(this);
}

void CompoundStatement::print(ostream& stream) const {
    stream << '[';
    Statement::print(stream);
    stream << "\"block\", " << nodes << ']';
}



ExprStatement::ExprStatement(Expr* expr)
    : Statement(expr->location), expr(expr) {
}

VisitStatementOutput ExprStatement::accept(Visitor& visitor) {
    return visitor.visit(this);
}

void ExprStatement::print(ostream& stream) const {
    stream << expr;
}



ForStatement::ForStatement(Declaration* declaration,  Expr* initialize, Expr* condition, Expr* iterate, Statement* body, const Location& location)
    : Statement(location), declaration(declaration), initialize(initialize), condition(condition), iterate(iterate), body(body) {
}

VisitStatementOutput ForStatement::accept(Visitor& visitor) {
    return visitor.visit(this);
}

void ForStatement::print(ostream& stream) const {
    stream << '[';
    Statement::print(stream);
    stream << "\"for\", " << declaration << ", " << initialize << ", " << condition << ", " << iterate << ", " << body << ']';
}


JumpStatement::JumpStatement(TokenKind kind, const Identifier& identifier, const Location& location)
    : Statement(location), kind(kind), identifier(identifier) {
}

VisitStatementOutput JumpStatement::accept(Visitor& visitor) {
    return visitor.visit(this);
}

void JumpStatement::print(ostream& stream) const {
    stream << '[';
    Statement::print(stream);

    switch (kind) {
      case TOK_GOTO:
        stream << "\"goto\"";
        break;
      case TOK_BREAK:
        stream << "\"break\"";
        break;
      case TOK_CONTINUE:
        stream << "\"continue\"";
        break;
    }

    stream << ", \"" << identifier << "\"]";
}

const char* JumpStatement::message_kind() const {
    switch (kind) {
      case TOK_GOTO:
        return "goto";
      case TOK_BREAK:
        return "break";
      case TOK_CONTINUE:
        return "continue";
    }
    return {};
}

IfElseStatement::IfElseStatement(Expr* condition, Statement* then_statement, Statement* else_statement, const Location& location)
    : Statement(location), condition(condition), then_statement(then_statement), else_statement(else_statement) {
}

VisitStatementOutput IfElseStatement::accept(Visitor& visitor) {
    return visitor.visit(this);
}

void IfElseStatement::print(ostream& stream) const {
    stream << '[';
    Statement::print(stream);
    stream << "\"if\", " << condition << ", " << then_statement << ", " << else_statement << ']';
}


ReturnStatement::ReturnStatement(Expr* expr, const Location& location)
    : Statement(location), expr(expr) {
}

VisitStatementOutput ReturnStatement::accept(Visitor& visitor) {
    return visitor.visit(this);
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


SwitchStatement::SwitchStatement(Expr* expr, CompoundStatement* body, const Location& location)
    : Statement(location), expr(expr), body(body) {
}

VisitStatementOutput SwitchStatement::accept(Visitor& visitor) {
    return visitor.visit(this);
}

void SwitchStatement::print(ostream& stream) const {
    stream << '[';
    Statement::print(stream);

    stream << "\"switch\", " << expr << ", " << body << ']';
}

