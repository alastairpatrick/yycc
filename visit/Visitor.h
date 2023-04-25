#ifndef VISIT_VISITOR_H
#define VISIT_VISITOR_H

#include "parse/ArrayType.h"
#include "parse/Constant.h"
#include "parse/Expr.h"
#include "parse/Statement.h"
#include "parse/Type.h"
#include "Value.h"

struct VisitDeclaratorInput {
    Declarator* secondary{};

    VisitDeclaratorInput() = default;
    explicit VisitDeclaratorInput(Declarator* secondary): secondary(secondary) {}
};

struct VisitDeclaratorOutput {
};

enum class ConvKind {
    IMPLICIT,
    EXPLICIT,
};

struct VisitTypeInput {
    Value value;
    const Type* dest_type{};
};

struct VisitTypeOutput {
    Value value;
    ConvKind conv_kind = ConvKind::IMPLICIT;

    VisitTypeOutput() = default;
    explicit VisitTypeOutput(Value value, ConvKind kind = ConvKind::IMPLICIT): value(value), conv_kind(kind) {}
    explicit VisitTypeOutput(const Type* type, LLVMValueRef value = nullptr, ConvKind kind = ConvKind::IMPLICIT): value(type, value), conv_kind(kind) {}
};

struct VisitStatementInput {
};

struct VisitStatementOutput {
    Value value;

    VisitStatementOutput() = default;
    explicit VisitStatementOutput(Value value): value(value) {}
    explicit VisitStatementOutput(const Type* type, LLVMValueRef value = nullptr): value(type, value) {}
};

struct Visitor {
    VisitDeclaratorOutput accept(Declarator* declarator, const VisitDeclaratorInput& input);
    virtual void pre_visit(Declarator* declarator);

    virtual VisitDeclaratorOutput visit(Declarator* declarator, Entity* entity, const VisitDeclaratorInput& input);
    virtual VisitDeclaratorOutput visit(Declarator* declarator, TypeDef* type_def, const VisitDeclaratorInput& input);
    virtual VisitDeclaratorOutput visit(Declarator* declarator, EnumConstant* enum_constant, const VisitDeclaratorInput& input);

    virtual VisitTypeOutput visit(const EnumType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const FloatingPointType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const FunctionType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const IntegerType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const PointerType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const QualifiedType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const ResolvedArrayType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const StructType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const TypeDefType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const TypeOfType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const VoidType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const UnboundType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const UnionType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const UniversalType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const UnqualifiedType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const UnresolvedArrayType* type, const VisitTypeInput& input);

    VisitStatementOutput accept(Statement* statement, const VisitStatementInput& input);
    virtual void pre_visit(Statement* statement);

    virtual VisitStatementOutput visit(CompoundStatement* statement, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(ForStatement* statement, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(GoToStatement* statement, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(IfElseStatement* statement, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(ReturnStatement* statement, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(SwitchStatement* statement, const VisitStatementInput& input);

    virtual VisitStatementOutput visit(AddressExpr* expr, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(BinaryExpr* expr, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(CallExpr* expr, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(CastExpr* expr, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(ConditionExpr* expr, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(DereferenceExpr* expr, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(EntityExpr* expr, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(IncDecExpr* expr, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(InitializerExpr* expr, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(SizeOfExpr* expr, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(SubscriptExpr* expr, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(UninitializedExpr* expr, const VisitStatementInput& input);

    virtual VisitStatementOutput visit(IntegerConstant* constant, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(FloatingPointConstant* constant, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(StringConstant* constant, const VisitStatementInput& input);

};

#endif

