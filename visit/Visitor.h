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

struct VisitStatementInput {
};

struct VisitStatementOutput {
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

