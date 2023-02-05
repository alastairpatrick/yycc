#ifndef DECL_H
#define DECL_H

#include "ASTNode.h"

struct Variable: Decl {
    Variable(StorageClass storage_class, const Type* type, const string* identifier, shared_ptr<Expr> initializer, const Location& location);

    shared_ptr<Expr> initializer;

    virtual void print(std::ostream& stream) const;
};

struct Function: Decl {
    Function(StorageClass storage_class, const FunctionType* type, const string* identifier, shared_ptr<Statement> body, const Location& location);

    shared_ptr<Statement> body;

    virtual bool is_function_definition() const;
    virtual void print(std::ostream& stream) const;
};

struct TypeDef: Decl {
    TypeDef(const Type* type, const string* identifier, const Location& location);

    virtual const Type* to_type() const;
    virtual void redeclare(const Decl* redeclared) const;
    virtual void print(std::ostream& stream) const;
};

#endif