#ifndef STMT_H
#define STMT_H

#include "ast.h"

struct CompoundStatement: Statement {
	CompoundStatement(DeclStatementList items, const Location& location);

	DeclStatementList items;

	virtual void print(std::ostream& stream) const;
};

struct ReturnStatement: Statement {
	ReturnStatement(shared_ptr<Expr> value, const Location& location);

	shared_ptr<Expr> value;

	virtual void print(std::ostream& stream) const;
};

#endif
