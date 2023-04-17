#ifndef PARSER_STATEMENT_H
#define PARSER_STATEMENT_H

#include "ASTNode.h"
#include "Scope.h"

struct CompoundStatement: Statement {
    Scope scope;
    ASTNodeVector nodes;

    CompoundStatement(Scope&& scope, ASTNodeVector&& items, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) override;
    virtual void print(ostream& stream) const;
};

struct ReturnStatement: Statement {
    Expr* expr{};

    ReturnStatement(Expr* expr, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) override;
    virtual void print(ostream& stream) const;
};

#endif
