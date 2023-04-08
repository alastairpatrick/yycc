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

    Location location;
    Fragment fragment;
    IdentifierScope scope{};
    StorageClass storage_class = StorageClass::NONE;
    const Type* type{};
    vector<Declarator*> declarators;
    bool mark_root{};

    virtual void print(ostream& stream) const;
};

// Most important is functions must have priority over variables so, when generating or reconciling #static and #extern identifiers,
// function is preferred in cases of ambiguity, e.g.:
// F foo;           // "F" typedef of funtion type in another translation unit. "foo" looks like a variable.
// void foo() {}    // Now "foo" is determined to be a function.
enum class DeclaratorKind {
    VARIABLE,
    FUNCTION,
    ENUM_CONSTANT,
    TYPE_DEF,
};

struct DeclaratorDelegate: ASTNode {
    explicit DeclaratorDelegate(Declarator* declarator);
    void operator=(const DeclaratorDelegate&) = delete;

    Declarator* const declarator;

    virtual DeclaratorKind kind() const = 0;
    virtual Linkage linkage() const;
    virtual const Type* to_type() const;
    virtual void compose(Declarator* later) = 0;
    virtual void print(ostream& stream) const = 0;
};

struct Variable: DeclaratorDelegate {
    explicit Variable(Declarator* declarator);
    Variable(Declarator* declarator, Expr* initializer, Expr* bit_field_size);

    StorageDuration storage_duration() const;

    Expr* initializer{};
    Expr* bit_field_size{};

    virtual DeclaratorKind kind() const;
    virtual Linkage linkage() const;
    virtual void compose(Declarator* later);
    virtual void print(ostream& stream) const;
};

struct Function: DeclaratorDelegate {
    explicit Function(Declarator* declarator);
    Function(Declarator* declarator, uint32_t specifiers, vector<Variable*>&& params, Statement* body);

    vector<Variable*> params;
    Statement* body{};

    bool inline_definition{};

    virtual DeclaratorKind kind() const;
    virtual Linkage linkage() const;
    virtual void compose(Declarator* later);
    virtual void print(ostream& stream) const;
};

struct TypeDef: DeclaratorDelegate {
    explicit TypeDef(Declarator* declarator);

    virtual DeclaratorKind kind() const;
    virtual const Type* to_type() const;
    virtual void compose(Declarator* later);
    virtual void print(ostream& stream) const;
};

struct EnumConstant: DeclaratorDelegate {
    explicit EnumConstant(Declarator* declarator);
    EnumConstant(Declarator* declarator, Expr* constant);

    Expr* constant{};

    virtual DeclaratorKind kind() const;
    virtual void compose(Declarator* later);
    virtual void print(ostream& stream) const;
};

#endif