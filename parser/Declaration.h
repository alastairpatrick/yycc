#ifndef PARSER_DECLARATION_H
#define PARSER_DECLARATION_H

#include "ASTNode.h"
#include "Type.h"

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

struct FunctionType;
struct ResolveContext;

ostream& operator<<(ostream& stream, Linkage linkage);
ostream& operator<<(ostream& stream, StorageDuration duration);

struct ResolveContext {
    unordered_set<Declarator*> todo;
};

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

enum class DeclaratorKind {
    ENTITY,
    ENUM_CONSTANT,
    TYPE_DEF,
};

struct DeclaratorDelegate: ASTNode {
    Declarator* const declarator;

    virtual DeclaratorKind kind() const = 0;
    virtual Linkage linkage() const;
    virtual const Type* to_type() const;
    virtual void compose(Declarator* later) = 0;
    virtual void print(ostream& stream) const = 0;

    explicit DeclaratorDelegate(Declarator* declarator);
    void operator=(const DeclaratorDelegate&) = delete;
};

struct Entity: DeclaratorDelegate {
    // Variable related
    StorageDuration storage_duration() const;
    Expr* initializer{};
    Expr* bit_field_size{};

    // Function related
    vector<Entity*> params;
    Statement* body{};
    bool inline_definition{};

    Entity(Declarator* declarator, Expr* initializer, Expr* bit_field_size);
    Entity(Declarator* declarator, uint32_t specifiers, vector<Entity*>&& params, Statement* body);
    explicit Entity(Declarator* declarator);

    bool is_function() const;

    virtual DeclaratorKind kind() const override;
    virtual Linkage linkage() const override;
    virtual void compose(Declarator* later) override;
    virtual void print(ostream& stream) const override;
};

struct TypeDef: DeclaratorDelegate {
    TypeDefType type_def_type;

    explicit TypeDef(Declarator* declarator);

    virtual DeclaratorKind kind() const override;
    virtual const Type* to_type() const override;
    virtual void compose(Declarator* later) override;
    virtual void print(ostream& stream) const override;
};

struct EnumConstant: DeclaratorDelegate {
    Identifier enum_tag;
    Expr* constant{};

    explicit EnumConstant(Declarator* declarator);
    EnumConstant(Declarator* declarator, const Identifier& enum_tag, Expr* constant);

    virtual DeclaratorKind kind() const override;
    virtual void compose(Declarator* later) override;
    virtual void print(ostream& stream) const override;
};

#endif