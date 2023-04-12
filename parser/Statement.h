#ifndef PARSER_STATEMENT_H
#define PARSER_STATEMENT_H

#include "ASTNode.h"

struct CompoundStatement: Statement {
    CompoundStatement(ASTNodeVector&& items, const Location& location);

    ASTNodeVector items;

    virtual void print(ostream& stream) const;
};

struct ReturnStatement: Statement {
    ReturnStatement(Expr* value, const Location& location);

    Expr* value{};

    virtual void print(ostream& stream) const;
};

#endif
