#ifndef PARSE_STATEMENT_H
#define PARSE_STATEMENT_H

#include "ASTNode.h"
#include "lex/Token.h"
#include "Scope.h"

struct CompoundStatement: Statement {
    ASTNodeVector nodes;

    CompoundStatement(ASTNodeVector&& items, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor) override;
    virtual void print(ostream& stream) const override;
};

struct ExprStatement: Statement {
    Expr* expr{};

    explicit ExprStatement(Expr* expr);
    virtual VisitStatementOutput accept(Visitor& visitor) override;
    virtual void print(ostream& stream) const override;
};

struct ForStatement: Statement {
    Declaration* declaration{};
    Expr* initialize{};
    Expr* condition{};
    Expr* iterate{};
    Statement* body{};

    ForStatement(Declaration* declaration, Expr* initialize, Expr* condition, Expr* iterate, Statement* body, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor) override;
    virtual void print(ostream& stream) const override;
};

struct JumpStatement: Statement {
    TokenKind kind;
    Identifier identifier;

    JumpStatement(TokenKind kind, const Identifier& identifier, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor) override;
    virtual void print(ostream& stream) const override;
    const char* message_kind() const;
};

struct IfElseStatement: Statement {
    Expr* condition{};
    Statement* then_statement{};
    Statement* else_statement{};

    IfElseStatement(Expr* condition, Statement* then_statement, Statement* else_statement, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor) override;
    virtual void print(ostream& stream) const override;
};

struct ReturnStatement: Statement {
    Expr* expr{};

    ReturnStatement(Expr* expr, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor) override;
    virtual void print(ostream& stream) const override;
};

struct SwitchStatement: Statement {
    Expr* expr{};
    Statement* body{};
    vector<Expr*> cases;
    int num_defaults{};

    SwitchStatement(Expr* expr, CompoundStatement* body, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor) override;
    virtual void print(ostream& stream) const override;
};

struct ThrowStatement: Statement {
    Expr* expr{};

    ThrowStatement(Expr* expr, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor) override;
    virtual void print(ostream& stream) const override;
};

struct TryStatement: Statement {
    Statement* try_statement;
    Statement* catch_statement;
    Declarator* declarator;

    TryStatement(Statement* try_statement, Declarator* declarator, Statement* catch_statement, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor) override;
    virtual void print(ostream& stream) const override;
};

#endif
