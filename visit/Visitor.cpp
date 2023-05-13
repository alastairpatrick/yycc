#include "Visitor.h"
#include "parse/Declaration.h"

/* Declarations */

VisitDeclaratorOutput Visitor::accept_declarator(Declarator* declarator) {
    if (!declarator) return VisitDeclaratorOutput();
    return declarator->accept(*this, VisitDeclaratorInput());
}

VisitDeclaratorOutput Visitor::visit(Declarator* declarator, Variable* variable, const VisitDeclaratorInput& input) {
    return VisitDeclaratorOutput();
}

VisitDeclaratorOutput Visitor::visit(Declarator* declarator, Function* function, const VisitDeclaratorInput& input) {
    return VisitDeclaratorOutput();
}

VisitDeclaratorOutput Visitor::visit(Declarator* declarator, TypeDelegate* type_delegate, const VisitDeclaratorInput& input) {
    return VisitDeclaratorOutput();
}

VisitDeclaratorOutput Visitor::visit(Declarator* declarator, EnumConstant* enum_constant, const VisitDeclaratorInput& input) {
    return VisitDeclaratorOutput();
}

/* Statements */

VisitStatementOutput Visitor::accept_statement(Statement* statement) {
    if (!statement) return VisitStatementOutput();
    return statement->accept(*this);
}

VisitStatementOutput Visitor::visit(CompoundStatement* statement) {
    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(ExprStatement* statement) {
    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(ForStatement* statement) {
    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(GoToStatement* statement) {
    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(IfElseStatement* statement) {
    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(ReturnStatement* statement) {
    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(SwitchStatement* statement) {
    return VisitStatementOutput();
}

/* Expressions */

[[nodiscard]]
VisitExpressionOutput Visitor::accept_expr(Expr* expr) {
    if (!expr) return VisitExpressionOutput();
    return expr->accept(*this);
}

VisitExpressionOutput Visitor::visit(AddressExpr* expr) {
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput Visitor::visit(BinaryExpr* expr) {
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput Visitor::visit(CallExpr* expr) {
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput Visitor::visit(CastExpr* expr) {
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput Visitor::visit(ConditionExpr* expr) {
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput Visitor::visit(DereferenceExpr* expr) {
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput Visitor::visit(EntityExpr* expr) {
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput Visitor::visit(IncDecExpr* expr) {
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput Visitor::visit(InitializerExpr* expr) {
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput Visitor::visit(MemberExpr* expr) {
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput Visitor::visit(SizeOfExpr* expr) {
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput Visitor::visit(SubscriptExpr* expr) {
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput Visitor::visit(UninitializedExpr* expr) {
    return VisitExpressionOutput(expr);
}

/* Constants */

VisitExpressionOutput Visitor::visit(IntegerConstant* constant) {
    return VisitExpressionOutput(constant);
}

VisitExpressionOutput Visitor::visit(FloatingPointConstant* constant) {
    return VisitExpressionOutput(constant);
}

VisitExpressionOutput Visitor::visit(StringConstant* constant) {
    return VisitExpressionOutput(constant);
}



