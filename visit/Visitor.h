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
    C_IMPLICIT, // conversions that need not be explicit in C
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

struct VisitExpressionInput {
};

struct VisitExpressionOutput {
    Value value;
    Expr* expr{};

    VisitExpressionOutput() = default;
    explicit VisitExpressionOutput(Expr* expr): expr(expr) {}
    explicit VisitExpressionOutput(Value value): value(value) {}
    explicit VisitExpressionOutput(const Type* type, LLVMValueRef value = nullptr): value(type, value) {}
};

struct Visitor {
    VisitDeclaratorOutput accept(Declarator* declarator, const VisitDeclaratorInput& input);
    virtual void pre_visit(Declarator* declarator);

    virtual VisitDeclaratorOutput visit(Declarator* declarator, Variable* variable, const VisitDeclaratorInput& input);
    virtual VisitDeclaratorOutput visit(Declarator* declarator, Function* function, const VisitDeclaratorInput& input);
    virtual VisitDeclaratorOutput visit(Declarator* declarator, TypeDelegate* type_delegate, const VisitDeclaratorInput& input);
    virtual VisitDeclaratorOutput visit(Declarator* declarator, EnumConstant* enum_constant, const VisitDeclaratorInput& input);

    virtual VisitTypeOutput visit(const EnumType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const FloatingPointType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const FunctionType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const IntegerType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const NestedType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const PointerType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const PassByReferenceType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const QualifiedType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const ResolvedArrayType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const StructType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const TypeDefType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const TypeOfType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const VoidType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const UnboundType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const UnionType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const UnqualifiedType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const UnresolvedArrayType* type, const VisitTypeInput& input);

    VisitStatementOutput accept(Statement* statement, const VisitStatementInput& input);
    virtual void pre_visit(Statement* statement);

    virtual VisitStatementOutput visit(CompoundStatement* statement, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(ExprStatement* statement, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(ForStatement* statement, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(GoToStatement* statement, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(IfElseStatement* statement, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(ReturnStatement* statement, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(SwitchStatement* statement, const VisitStatementInput& input);

    VisitExpressionOutput accept(Expr* expr, const VisitExpressionInput& input);

    virtual VisitExpressionOutput visit(AddressExpr* expr, const VisitExpressionInput& input);
    virtual VisitExpressionOutput visit(BinaryExpr* expr, const VisitExpressionInput& input);
    virtual VisitExpressionOutput visit(CallExpr* expr, const VisitExpressionInput& input);
    virtual VisitExpressionOutput visit(CastExpr* expr, const VisitExpressionInput& input);
    virtual VisitExpressionOutput visit(ConditionExpr* expr, const VisitExpressionInput& input);
    virtual VisitExpressionOutput visit(DereferenceExpr* expr, const VisitExpressionInput& input);
    virtual VisitExpressionOutput visit(EntityExpr* expr, const VisitExpressionInput& input);
    virtual VisitExpressionOutput visit(IncDecExpr* expr, const VisitExpressionInput& input);
    virtual VisitExpressionOutput visit(InitializerExpr* expr, const VisitExpressionInput& input);
    virtual VisitExpressionOutput visit(MemberExpr* expr, const VisitExpressionInput& input);
    virtual VisitExpressionOutput visit(SizeOfExpr* expr, const VisitExpressionInput& input);
    virtual VisitExpressionOutput visit(SubscriptExpr* expr, const VisitExpressionInput& input);
    virtual VisitExpressionOutput visit(UninitializedExpr* expr, const VisitExpressionInput& input);

    virtual VisitExpressionOutput visit(IntegerConstant* constant, const VisitExpressionInput& input);
    virtual VisitExpressionOutput visit(FloatingPointConstant* constant, const VisitExpressionInput& input);
    virtual VisitExpressionOutput visit(StringConstant* constant, const VisitExpressionInput& input);

};

#endif

