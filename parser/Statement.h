#ifndef PARSER_STATEMENT_H
#define PARSER_STATEMENT_H

#include "ASTNode.h"

struct CompoundStatement: Statement {
    ASTNodeVector items;

    CompoundStatement(ASTNodeVector&& items, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) override;
    virtual void print(ostream& stream) const;
};

struct ReturnStatement: Statement {
    Expr* value{};

    ReturnStatement(Expr* value, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) override;
    virtual void print(ostream& stream) const;
};

#endif
