#ifndef PARSE_STATEMENT_H
#define PARSE_STATEMENT_H

#include "ASTNode.h"
#include "Scope.h"

struct CompoundStatement: Statement {
    Scope scope;
    ASTNodeVector nodes;

    CompoundStatement(Scope&& scope, ASTNodeVector&& items, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) override;
    virtual void print(ostream& stream) const override;
};

struct ForStatement: Statement {
    // Only one of declatation or initialize may be non-null
    Declaration* declaration{};
    Expr* initialize{};
    Expr* condition{};
    Expr* iterate{};
    Statement* body{};

    ForStatement(Declaration* declaration, Expr* initialize, Expr* condition, Expr* iterate, Statement* body, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) override;
    virtual void print(ostream& stream) const override;
};

struct GoToStatement: Statement {
    Identifier identifier;

    GoToStatement(const Identifier& identifier, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) override;
    virtual void print(ostream& stream) const override;
};

struct IfElseStatement: Statement {
    Expr* condition{};
    Statement* then_statement{};
    Statement* else_statement{};

    IfElseStatement(Expr* condition, Statement* then_statement, Statement* else_statement, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) override;
    virtual void print(ostream& stream) const override;
};

struct ReturnStatement: Statement {
    Expr* expr{};

    ReturnStatement(Expr* expr, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) override;
    virtual void print(ostream& stream) const override;
};

#endif
