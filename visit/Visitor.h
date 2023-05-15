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

struct VisitStatementOutput {
    Statement* statement{};

    VisitStatementOutput() = default;
    explicit VisitStatementOutput(Statement* statement): statement(statement) {}
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
    typedef Visitor Base;

    virtual VisitDeclaratorOutput accept_declarator(Declarator* declarator);
    virtual VisitDeclaratorOutput visit(Declarator* declarator, Variable* variable, const VisitDeclaratorInput& input);
    virtual VisitDeclaratorOutput visit(Declarator* declarator, Function* function, const VisitDeclaratorInput& input);
    virtual VisitDeclaratorOutput visit(Declarator* declarator, TypeDelegate* type_delegate, const VisitDeclaratorInput& input);
    virtual VisitDeclaratorOutput visit(Declarator* declarator, EnumConstant* enum_constant, const VisitDeclaratorInput& input);

    virtual VisitStatementOutput accept_statement(Statement* statement);
    virtual VisitStatementOutput visit(CompoundStatement* statement);
    virtual VisitStatementOutput visit(ExprStatement* statement);
    virtual VisitStatementOutput visit(ForStatement* statement);
    virtual VisitStatementOutput visit(GoToStatement* statement);
    virtual VisitStatementOutput visit(IfElseStatement* statement);
    virtual VisitStatementOutput visit(ReturnStatement* statement);
    virtual VisitStatementOutput visit(SwitchStatement* statement);

    virtual VisitExpressionOutput accept_expr(Expr* expr);
    virtual VisitExpressionOutput visit(AddressExpr* expr);
    virtual VisitExpressionOutput visit(BinaryExpr* expr);
    virtual VisitExpressionOutput visit(CallExpr* expr);
    virtual VisitExpressionOutput visit(CastExpr* expr);
    virtual VisitExpressionOutput visit(ConditionExpr* expr);
    virtual VisitExpressionOutput visit(DereferenceExpr* expr);
    virtual VisitExpressionOutput visit(EntityExpr* expr);
    virtual VisitExpressionOutput visit(IncDecExpr* expr);
    virtual VisitExpressionOutput visit(InitializerExpr* expr);
    virtual VisitExpressionOutput visit(MemberExpr* expr);
    virtual VisitExpressionOutput visit(SizeOfExpr* expr);
    virtual VisitExpressionOutput visit(SubscriptExpr* expr);
    virtual VisitExpressionOutput visit(UnaryExpr* expr);
    virtual VisitExpressionOutput visit(UninitializedExpr* expr);

    virtual VisitExpressionOutput visit(IntegerConstant* constant);
    virtual VisitExpressionOutput visit(FloatingPointConstant* constant);
    virtual VisitExpressionOutput visit(StringConstant* constant);

};

#endif

