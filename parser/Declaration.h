#ifndef AST_DECLARATION_H
#define AST_DECLARATION_H

#include "ASTNode.h"
#include "Type.h"

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
struct ResolutionContext;

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

    void resolve(ResolutionContext& context);

    virtual void print(ostream& stream) const;
};

enum class DeclaratorKind {
    ENTITY,
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
    virtual void compose(Declarator* other) = 0;
    virtual void print(ostream& stream) const = 0;
};

struct Entity: DeclaratorDelegate {
    Entity(Declarator* declarator, Expr* initializer, Expr* bit_field_size);
    Entity(Declarator* declarator, uint32_t specifiers, vector<Entity*>&& params, Statement* body);
    explicit Entity(Declarator* declarator);

    // Variable related
    StorageDuration storage_duration() const;
    Expr* initializer{};
    Expr* bit_field_size{};

    // Function related
    vector<Entity*> params;
    Statement* body{};
    bool inline_definition{};

    bool is_function() const;

    virtual DeclaratorKind kind() const;
    virtual Linkage linkage() const;
    virtual void compose(Declarator* other);
    virtual void print(ostream& stream) const;
};

struct TypeDef: DeclaratorDelegate {
    explicit TypeDef(Declarator* declarator);

    TypeDefType type_def_type;

    virtual DeclaratorKind kind() const;
    virtual const Type* to_type() const;
    virtual void compose(Declarator* other);
    virtual void print(ostream& stream) const;
};

struct EnumConstant: DeclaratorDelegate {
    explicit EnumConstant(Declarator* declarator);
    EnumConstant(Declarator* declarator, Expr* constant);

    Expr* constant{};

    virtual DeclaratorKind kind() const;
    virtual void compose(Declarator* other);
    virtual void print(ostream& stream) const;
};

#endif