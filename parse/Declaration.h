#ifndef PARSE_DECLARATION_H
#define PARSE_DECLARATION_H

#include "ASTNode.h"
#include "Scope.h"
#include "Type.h"
#include "Value.h"

enum class DeclaratorKind {
    ENTITY,
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


struct Declaration: ASTNode {
    Location location;
    Fragment fragment;
    IdentifierScope scope{};
    StorageClass storage_class = StorageClass::NONE;
    const Type* type{};
    vector<Declarator*> declarators;
    bool mark_root{};

    Declaration(IdentifierScope scope, StorageClass storage_class, const Type* type, const Location& location);
    Declaration(IdentifierScope scope, const Location& location);

    virtual void print(ostream& stream) const override;
};

struct DeclaratorDelegate: ASTNode {
    Declarator* declarator;

    virtual DeclaratorKind kind() const = 0;
    virtual const char* error_kind() const = 0;
    virtual bool is_definition() const = 0;
    virtual Linkage linkage() const;
    virtual const Type* to_type() const;
    virtual VisitDeclaratorOutput accept(Visitor& visitor, const VisitDeclaratorInput& input) = 0;
    virtual void print(ostream& stream) const = 0;

    explicit DeclaratorDelegate(Declarator* declarator);
    void operator=(const DeclaratorDelegate&) = delete;
};

struct Entity: DeclaratorDelegate {
    Value value;

    // Variable related
    StorageDuration storage_duration() const;
    Expr* initializer{};
    Expr* bit_field_size{};
    size_t aggregate_index{};

    // Function related
    vector<Declarator*> parameters;
    Statement* body{};
    bool inline_definition{};

    Entity(Declarator* declarator, Expr* initializer, Expr* bit_field_size);
    Entity(Declarator* declarator, uint32_t specifiers, vector<Declarator*>&& parameters, Statement* body);
    explicit Entity(Declarator* declarator);

    bool is_function() const;

    virtual DeclaratorKind kind() const override;
    virtual const char* error_kind() const override;
    virtual bool is_definition() const override;
    virtual Linkage linkage() const override;
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
    Declarator* enum_tag{};
    Expr* constant_expr{};
    long long constant_int{};

    explicit EnumConstant(Declarator* declarator);
    EnumConstant(Declarator* declarator, Declarator* enum_tag, Expr* constant);

    virtual DeclaratorKind kind() const override;
    virtual const char* error_kind() const override;
    virtual bool is_definition() const override;
    virtual VisitDeclaratorOutput accept(Visitor& visitor, const VisitDeclaratorInput& input) override;
    virtual void print(ostream& stream) const override;
};

#endif