#ifndef VISIT_DEPTH_FIRST_VISITOR_H
#define VISIT_DEPTH_FIRST_VISITOR_H

#include "Visitor.h"

struct DepthFirstVisitor: Visitor {
    typedef DepthFirstVisitor Base;

    virtual VisitDeclaratorOutput visit(Declarator* declarator, Variable* variable, const VisitDeclaratorInput& input) override;
    virtual VisitDeclaratorOutput visit(Declarator* declarator, Function* function, const VisitDeclaratorInput& input) override;
    virtual VisitDeclaratorOutput visit(Declarator* declarator, TypeDelegate* type_delegate, const VisitDeclaratorInput& input) override;
    virtual VisitDeclaratorOutput visit(Declarator* declarator, EnumConstant* enum_constant, const VisitDeclaratorInput& input) override;

    virtual VisitStatementOutput accept_statement(Statement* statement);
    virtual VisitStatementOutput visit(CompoundStatement* statement) override;
    virtual VisitStatementOutput visit(ExprStatement* statement) override;
    virtual VisitStatementOutput visit(ForStatement* statement) override;
    virtual VisitStatementOutput visit(GoToStatement* statement) override;
    virtual VisitStatementOutput visit(IfElseStatement* statement) override;
    virtual VisitStatementOutput visit(ReturnStatement* statement) override;
    virtual VisitStatementOutput visit(SwitchStatement* statement) override;

    virtual VisitExpressionOutput visit(AddressExpr* expr) override;
    virtual VisitExpressionOutput visit(BinaryExpr* expr) override;
    virtual VisitExpressionOutput visit(CallExpr* expr) override;
    virtual VisitExpressionOutput visit(CastExpr* expr) override;
    virtual VisitExpressionOutput visit(ConditionExpr* expr) override;
    virtual VisitExpressionOutput visit(DereferenceExpr* expr) override;
    virtual VisitExpressionOutput visit(EntityExpr* expr) override;
    virtual VisitExpressionOutput visit(IncDecExpr* expr) override;
    virtual VisitExpressionOutput visit(InitializerExpr* expr) override;
    virtual VisitExpressionOutput visit(MemberExpr* expr) override;
    virtual VisitExpressionOutput visit(SizeOfExpr* expr) override;
    virtual VisitExpressionOutput visit(SubscriptExpr* expr) override;
    virtual VisitExpressionOutput visit(UnaryExpr* expr) override;
    virtual VisitExpressionOutput visit(UninitializedExpr* expr) override;

    virtual VisitExpressionOutput visit(IntegerConstant* constant) override;
    virtual VisitExpressionOutput visit(FloatingPointConstant* constant) override;
    virtual VisitExpressionOutput visit(StringConstant* constant) override;

};

#endif