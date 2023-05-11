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
    AGGREGATE,  // same duration as aggregate typed object of which this is a part
};

ostream& operator<<(ostream& stream, Linkage linkage);
ostream& operator<<(ostream& stream, StorageDuration duration);


struct Declaration: LocationNode {
    StorageClass storage_class = StorageClass::NONE;
    const Type* type{};
    vector<Declarator*> declarators;

    Declaration(StorageClass storage_class, const Type* type, const Location& location);
    explicit Declaration(const Location& location);

    virtual void print(ostream& stream) const override;
};

ostream& operator<<(ostream& stream, const vector<Declaration*>& items);

struct DeclaratorDelegate: ASTNode {
    virtual DeclaratorKind kind() const = 0;
    virtual const char* message_kind() const = 0;
    virtual bool message_is_definition() const = 0;
    virtual const Type* to_type() const;
    virtual VisitDeclaratorOutput accept(Declarator* declarator, Visitor& visitor, const VisitDeclaratorInput& input) = 0;
    virtual void print(const Declarator* declarator, ostream& stream) const = 0;

    void operator=(const DeclaratorDelegate&) = delete;
};

struct Entity: DeclaratorDelegate {
    Linkage linkage;
    Value value;

    explicit Entity(Linkage linkage);
};

struct BitField: Printable {
    Expr* expr;
    LLVMTypeRef storage_type{};
    LLVMValueRef bits_to_left{};
    LLVMValueRef bits_to_right{};
    LLVMValueRef mask{};

    explicit BitField(Expr* expr);
    virtual void print(ostream& stream) const override;
};

struct MemberVariable {
    unique_ptr<BitField> bit_field{};
    vector<LLVMValueRef> gep_indices{};
};

struct Variable: Entity {
    StorageDuration storage_duration{};
    Expr* initializer{};
    unique_ptr<MemberVariable> member{};

    Variable(Linkage linkage, StorageDuration storage_duration, Expr* initializer = nullptr);

    virtual DeclaratorKind kind() const override;
    virtual const char* message_kind() const override;
    virtual bool message_is_definition() const override;
    virtual VisitDeclaratorOutput accept(Declarator* declarator, Visitor& visitor, const VisitDeclaratorInput& input) override;
    virtual void print(const Declarator* declarator, ostream& stream) const override;
};

struct Function: Entity {
    vector<Declarator*> parameters;
    Statement* body{};
    bool inline_definition{};

    Function(Linkage linkage, bool inline_definition, vector<Declarator*>&& parameters, Statement* body);
    explicit Function(Linkage linkage);

    virtual DeclaratorKind kind() const override;
    virtual const char* message_kind() const override;
    virtual bool message_is_definition() const override;
    virtual VisitDeclaratorOutput accept(Declarator* declarator, Visitor& visitor, const VisitDeclaratorInput& input) override;
    virtual void print(const Declarator* declarator, ostream& stream) const override;
};

struct TypeDelegate: DeclaratorDelegate {
    TypeDefType type_def_type;

    TypeDelegate() = default;

    virtual DeclaratorKind kind() const override;
    virtual const char* message_kind() const override;
    virtual const Type* to_type() const override;
    virtual bool message_is_definition() const override;
    virtual VisitDeclaratorOutput accept(Declarator* declarator, Visitor& visitor, const VisitDeclaratorInput& input) override;
    virtual void print(const Declarator* declarator, ostream& stream) const override;
};

struct EnumConstant: DeclaratorDelegate {
    const EnumType* type{};
    Expr* expr{};
    bool ready{};
    long long value{};

    EnumConstant(const EnumType* type, Expr* constant);

    virtual DeclaratorKind kind() const override;
    virtual const char* message_kind() const override;
    virtual bool message_is_definition() const override;
    virtual VisitDeclaratorOutput accept(Declarator* declarator, Visitor& visitor, const VisitDeclaratorInput& input) override;
    virtual void print(const Declarator* declarator, ostream& stream) const override;
};

#endif