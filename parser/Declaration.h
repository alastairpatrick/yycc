#ifndef AST_DECLARATION_H
#define AST_DECLARATION_H

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
    explicit Declaration(IdentifierScope scope, StorageClass storage_class, const Type* base_type, const Location& location);

    IdentifierScope scope;
    StorageClass storage_class;
    Linkage linkage;
    const Type* base_type;
    vector<Declarator*> declarators;
    bool mark_root{};

    virtual void print(ostream& stream) const;
};

struct Variable: Declarator {
    Variable(const Declaration* declaration, const Type* type, const Identifier& identifier, Expr* initializer, const Location& location);

    StorageDuration storage_duration;
    Expr* initializer{};

    virtual void combine();
    virtual void print(ostream& stream) const;
};

struct Function: Declarator {
    Function(const Declaration* declaration, const FunctionType* type, uint32_t specifiers, const Identifier& identifier, vector<Variable*>&& params, Statement* body, const Location& location);

    vector<Variable*> params;
    Statement* body{};

    bool inline_definition;

    virtual void combine();
    virtual void print(ostream& stream) const;
};

struct TypeDef: Declarator {
    TypeDef(const Declaration* declaration, const Type* type, const Identifier& identifier, const Location& location);

    virtual const Type* to_type() const;
    virtual void print(ostream& stream) const;
};

struct EnumConstant: Declarator {
    EnumConstant(const Identifier& identifier, Expr* constant, const Location& location);

    Expr* constant{};

    virtual void print(ostream& stream) const;
};

#endif