#ifndef PARSE_DECLARATION_H
#define PARSE_DECLARATION_H

#include "ASTNode.h"
#include "Scope.h"
#include "Type.h"
#include "Value.h"

enum class DeclaratorKind {
    VARIABLE,
    FUNCTION,
    ENUM_CONSTANT,
    TYPE_DEF,
};

enum class IdentifierScope {
    FILE,
    BLOCK,
    PROTOTYPE,
    STRUCTURED,
    EXPRESSION,  // e.g. sizeof, typeof
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

ostream& operator<<(ostream& stream, Linkage linkage);
ostream& operator<<(ostream& stream, StorageDuration duration);


struct Declaration: LocationNode {
    Fragment fragment;
    IdentifierScope scope{};
    StorageClass storage_class = StorageClass::NONE;
    const Type* type{};
    vector<Declarator*> declarators;

    Declaration(IdentifierScope scope, StorageClass storage_class, const Type* type, const Location& location);
    Declaration(IdentifierScope scope, const Location& location);

    virtual void print(ostream& stream) const override;
};

ostream& operator<<(ostream& stream, const vector<Declaration*>& items);

struct DeclaratorDelegate: ASTNode {
    Declarator* declarator;

    virtual DeclaratorKind kind() const = 0;
    virtual const char* error_kind() const = 0;
    virtual bool is_definition() const = 0;
    virtual const Type* to_type() const;
    virtual VisitDeclaratorOutput accept(Visitor& visitor, const VisitDeclaratorInput& input) = 0;
    virtual void print(ostream& stream) const = 0;

    explicit DeclaratorDelegate(Declarator* declarator);
    void operator=(const DeclaratorDelegate&) = delete;
};

struct Entity: DeclaratorDelegate {
    Linkage linkage;
    Value value;

    Entity(Declarator* declarator, Linkage linkage);
};

struct BitField: ASTNode {
    Expr* expr;
    LLVMTypeRef storage_type{};
    LLVMValueRef bits_to_left{};
    LLVMValueRef bits_to_right{};
    LLVMValueRef mask{};

    explicit BitField(Expr* expr);
    virtual void print(ostream& stream) const override;
};

struct Variable: Entity {
    StorageDuration storage_duration{};
    Expr* initializer{};
    BitField* bit_field{};
    size_t aggregate_index{};

    Variable(Declarator* declarator, Linkage linkage, StorageDuration storage_duration, Expr* initializer = nullptr, Expr* bit_field_size = nullptr);

    virtual DeclaratorKind kind() const override;
    virtual const char* error_kind() const override;
    virtual bool is_definition() const override;
    virtual VisitDeclaratorOutput accept(Visitor& visitor, const VisitDeclaratorInput& input) override;
    virtual void print(ostream& stream) const override;
};

struct Function: Entity {
    vector<Declarator*> parameters;
    Statement* body{};
    bool inline_definition{};

    Function(Declarator* declarator, Linkage linkage, bool inline_definition, vector<Declarator*>&& parameters, Statement* body);
    Function(Declarator* declarator, Linkage linkage);

    virtual DeclaratorKind kind() const override;
    virtual const char* error_kind() const override;
    virtual bool is_definition() const override;
    virtual VisitDeclaratorOutput accept(Visitor& visitor, const VisitDeclaratorInput& input) override;
    virtual void print(ostream& stream) const override;
};

struct TypeDef: DeclaratorDelegate {
    TypeDefType type_def_type;

    explicit TypeDef(Declarator* declarator);

    virtual DeclaratorKind kind() const override;
    virtual const char* error_kind() const override;
    virtual const Type* to_type() const override;
    virtual bool is_definition() const override;
    virtual VisitDeclaratorOutput accept(Visitor& visitor, const VisitDeclaratorInput& input) override;
    virtual void print(ostream& stream) const override;
};

struct EnumConstant: DeclaratorDelegate {
    const EnumType* type{};
    Expr* expr{};
    bool ready{};
    long long value{};

    explicit EnumConstant(Declarator* declarator);
    EnumConstant(Declarator* declarator, const EnumType* type, Expr* constant);

    virtual DeclaratorKind kind() const override;
    virtual const char* error_kind() const override;
    virtual bool is_definition() const override;
    virtual VisitDeclaratorOutput accept(Visitor& visitor, const VisitDeclaratorInput& input) override;
    virtual void print(ostream& stream) const override;
};

#endif