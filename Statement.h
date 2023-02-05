#ifndef STATEMENT_H
#define STATEMENT_H

#include "ASTNode.h"

struct CompoundStatement: Statement {
    CompoundStatement(ASTNodeVector items, const Location& location);

    ASTNodeVector items;

    virtual void print(std::ostream& stream) const;
};

struct ReturnStatement: Statement {
    ReturnStatement(Expr* value, const Location& location);

    Expr* value{};

    virtual void print(std::ostream& stream) const;
};

#endif
