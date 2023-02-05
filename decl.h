#ifndef DECL_H
#define DECL_H

#include "ASTNode.h"

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
    Variable(StorageClass storage_class, const Type* type, const string* identifier, Expr* initializer, const Location& location);

    Expr* initializer{};

    virtual DeclKind kind() const;
    virtual void print(std::ostream& stream) const;
};

struct Function: Decl {
    Function(StorageClass storage_class, const FunctionType* type, const string* identifier, Statement* body, const Location& location);

    Statement* body{};

    virtual DeclKind kind() const;
    virtual bool is_function_definition() const;
    virtual void print(std::ostream& stream) const;
};

struct TypeDef: Decl {
    TypeDef(const Type* type, const string* identifier, const Location& location);

    virtual DeclKind kind() const;
    virtual const Type* to_type() const;
    virtual void redeclare(const Decl* redeclared) const;
    virtual void print(std::ostream& stream) const;
};

#endif