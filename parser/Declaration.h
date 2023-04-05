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
    Declaration(IdentifierScope scope, StorageClass storage_class, const Location& location);
    Declaration(IdentifierScope scope, const Location& location);
    void initialize(StorageClass storage_class);

    IdentifierScope scope;
    StorageClass storage_class{};
    Linkage linkage{};
    vector<Declarator*> declarators;
    bool mark_root{};

    virtual void print(ostream& stream) const;
};

struct Variable: Declarator {
    Variable(const Declaration* declaration, const Type* type, const Identifier& identifier, Expr* initializer, Expr* bit_field_size, const Location& location);

    StorageDuration storage_duration;
    Expr* initializer{};
    Expr* bit_field_size{};

    virtual void compose(Declarator* later);
    virtual void print(ostream& stream) const;
};

struct Function: Declarator {
    Function(const Declaration* declaration, const FunctionType* type, uint32_t specifiers, const Identifier& identifier, vector<Variable*>&& params, Statement* body, const Location& location);

    vector<Variable*> params;
    Statement* body{};

    bool inline_definition;

    virtual void compose(Declarator* later);
    virtual void print(ostream& stream) const;
};

struct TypeDef: Declarator {
    TypeDef(const Declaration* declaration, const Type* type, const Identifier& identifier, const Location& location);

    virtual const Type* to_type() const;
    virtual void print(ostream& stream) const;
};

struct EnumConstant: Declarator {
    EnumConstant(Declaration* declaration, const Identifier& identifier, Expr* constant, const Location& location);

    Expr* constant{};

    virtual void print(ostream& stream) const;
};

#endif