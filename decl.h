#ifndef DECL_H
#define DECL_H

#include "ASTNode.h"

// Function prototypes are represented in the AST as variables of function type. N.B.: _not_ function _pointer_ type!
// The reason is because before symbols have been resolved, a function prototype can be syntactically indistinguishable
// from a variable declaration. Example:
//
// typedef int compare(int, int);
// compare min, max;
//
// Above is equivalent to:
//
// typedef int compare(int, int);
// int min(int, int);
// int max(int, int);


struct Variable: Decl {
    Variable(StorageClass storage_class, const Type* type, std::string identifier, shared_ptr<Expr> initializer, const Location& location);

    shared_ptr<Expr> initializer;

    virtual void print(std::ostream& stream) const;
};

struct Function: Decl {
    Function(StorageClass storage_class, const FunctionType* type, std::string identifier, shared_ptr<Statement> body, const Location& location);

    shared_ptr<Statement> body;

    virtual void print(std::ostream& stream) const;
};

struct TypeDef: Decl {
    TypeDef(const Type* type, std::string identifier, const Location& location);

    shared_ptr<Statement> body;

    virtual void print(std::ostream& stream) const;
};

#endif