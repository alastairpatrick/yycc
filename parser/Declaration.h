#ifndef AST_DECLARATION_H
#define AST_DECLARATION_H

#include "ASTNode.h"

enum class IdentifierScope {
    FILE,
    BLOCK,
    PROTOTYPE,
    STRUCTURED,
};

enum class StorageClass {
    NONE,
    TYPEDEF,
    EXTERN,
    STATIC,
    AUTO,
    REGISTER,
};

enum class Linkage {
    NONE,
    INTERNAL,
    EXTERNAL,
};

enum class StorageDuration {
    AUTO,
    STATIC,
};

struct FunctionType;

ostream& operator<<(ostream& stream, Linkage linkage);
ostream& operator<<(ostream& stream, StorageDuration duration);

struct Declaration: ASTNode {
    Declaration(IdentifierScope scope, StorageClass storage_class, const Type* type, const Location& location);
    Declaration(IdentifierScope scope, const Location& location);

    Linkage linkage() const;

    Location location;
    Fragment fragment;
    IdentifierScope scope{};
    StorageClass storage_class = StorageClass::NONE;
    const Type* type{};
    vector<Declarator*> declarators;
    bool mark_root{};

    virtual void print(ostream& stream) const;
};

struct DeclaratorKind: ASTNode {
    explicit DeclaratorKind(Declarator* declarator);
    void operator=(const DeclaratorKind&) = delete;

    Declarator* const declarator;

    virtual const Type* to_type() const;
    virtual void compose(Declarator* later) = 0;
    virtual void print(ostream& stream) const = 0;
};

struct Variable: DeclaratorKind {
    explicit Variable(Declarator* declarator);
    Variable(Declarator* declarator, Expr* initializer, Expr* bit_field_size);

    StorageDuration storage_duration() const;

    Expr* initializer{};
    Expr* bit_field_size{};

    virtual void compose(Declarator* later);
    virtual void print(ostream& stream) const;
};

struct Function: DeclaratorKind {
    explicit Function(Declarator* declarator);
    Function(Declarator* declarator, uint32_t specifiers, vector<Variable*>&& params, Statement* body);

    vector<Variable*> params;
    Statement* body{};

    bool inline_definition{};

    virtual void compose(Declarator* later);
    virtual void print(ostream& stream) const;
};

struct TypeDef: DeclaratorKind {
    explicit TypeDef(Declarator* declarator);

    virtual const Type* to_type() const;
    virtual void compose(Declarator* later);
    virtual void print(ostream& stream) const;
};

struct EnumConstant: DeclaratorKind {
    explicit EnumConstant(Declarator* declarator);
    EnumConstant(Declarator* declarator, Expr* constant);

    Expr* constant{};

    virtual void compose(Declarator* later);
    virtual void print(ostream& stream) const;
};

#endif