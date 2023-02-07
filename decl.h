#ifndef DECL_H
#define DECL_H

#include "ASTNode.h"

enum class IdentifierScope {
    FILE,
    BLOCK,
    PROTOTYPE,
};

enum class StorageClass {
    NONE,
    TYPEDEF,
    EXTERN,
    STATIC,
    AUTO,
    REGISTER,
};

ostream& operator<<(ostream& stream, StorageClass storage_class);

enum class DeclKind {
    VARIABLE,
    FUNCTION,
    TYPEDEF,
};

struct Variable: Decl {
    Variable(IdentifierScope scope, StorageClass storage_class, const Type* type, const string* identifier, Expr* initializer, const Location& location);

    Expr* initializer{};

    virtual DeclKind kind() const;
    virtual void print(std::ostream& stream) const;
};

struct Function: Decl {
    Function(IdentifierScope scope, StorageClass storage_class, const FunctionType* type, const string* identifier, vector<Variable*> params, Statement* body, const Location& location);

    vector<Variable*> params;
    Statement* body{};

    virtual DeclKind kind() const;
    virtual bool is_function_definition() const;
    virtual void redeclare(Decl* redeclared);
    virtual void print(std::ostream& stream) const;
};

struct TypeDef: Decl {
    TypeDef(IdentifierScope scope, const Type* type, const string* identifier, const Location& location);

    virtual DeclKind kind() const;
    virtual const Type* to_type() const;
    virtual void print(std::ostream& stream) const;
};

#endif