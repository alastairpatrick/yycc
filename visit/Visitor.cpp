#include "Visitor.h"

/* Declarations */

VisitDeclaratorOutput Visitor::visit_default(Declarator* declarator, const VisitDeclaratorInput& input) {
    return VisitDeclaratorOutput();
}

VisitDeclaratorOutput Visitor::visit(Declarator* declarator, Entity* entity, const VisitDeclaratorInput& input) {
    return visit_default(declarator, input);
}

VisitDeclaratorOutput Visitor::visit(Declarator* declarator, TypeDef* type_def, const VisitDeclaratorInput& input) {
    return visit_default(declarator, input);
}

VisitDeclaratorOutput Visitor::visit(Declarator* declarator, EnumConstant* enum_constant, const VisitDeclaratorInput& input) {
    return visit_default(declarator, input);
}

/* Types */

VisitTypeOutput Visitor::visit_default(const Type* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput Visitor::visit(const VoidType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

VisitTypeOutput Visitor::visit(const UniversalType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

VisitTypeOutput Visitor::visit(const IntegerType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

VisitTypeOutput Visitor::visit(const FloatingPointType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

VisitTypeOutput Visitor::visit(const PointerType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

VisitTypeOutput Visitor::visit(const QualifiedType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

VisitTypeOutput Visitor::visit(const UnqualifiedType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

VisitTypeOutput Visitor::visit(const FunctionType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

VisitTypeOutput Visitor::visit(const ResolvedArrayType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

VisitTypeOutput Visitor::visit(const StructType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

VisitTypeOutput Visitor::visit(const UnionType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

VisitTypeOutput Visitor::visit(const EnumType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

VisitTypeOutput Visitor::visit(const TypeOfType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

VisitTypeOutput Visitor::visit(const UnboundType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

VisitTypeOutput Visitor::visit(const UnresolvedArrayType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

VisitTypeOutput Visitor::visit(const TypeDefType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

/* Statements */

VisitStatementOutput Visitor::accept(Statement* statement, const VisitStatementInput& input) {
    pre_visit(statement);
    return statement->accept(*this, input);
}

void Visitor::pre_visit(Statement* statement) {
}

VisitStatementOutput Visitor::visit_default(Statement* statement, const VisitStatementInput& input) {
    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(CompoundStatement* statement, const VisitStatementInput& input) {
    return visit_default(statement, input);
}

VisitStatementOutput Visitor::visit(ForStatement* statement, const VisitStatementInput& input) {
    return visit_default(statement, input);
}

VisitStatementOutput Visitor::visit(GoToStatement* statement, const VisitStatementInput& input) {
    return visit_default(statement, input);
}

VisitStatementOutput Visitor::visit(IfElseStatement* statement, const VisitStatementInput& input) {
    return visit_default(statement, input);
}

VisitStatementOutput Visitor::visit(ReturnStatement* statement, const VisitStatementInput& input) {
    return visit_default(statement, input);
}

VisitStatementOutput Visitor::visit(SwitchStatement* statement, const VisitStatementInput& input) {
    return visit_default(statement, input);
}

/* Expressions */

VisitStatementOutput Visitor::visit_default(Expr* expr, const VisitStatementInput& input) {
    return visit_default((Statement*) expr, input);
}

VisitStatementOutput Visitor::visit(AddressExpr* expr, const VisitStatementInput& input) {
    return visit_default(expr, input);
}

VisitStatementOutput Visitor::visit(BinaryExpr* expr, const VisitStatementInput& input) {
    return visit_default(expr, input);
}

VisitStatementOutput Visitor::visit(CallExpr* expr, const VisitStatementInput& input) {
    return visit_default(expr, input);
}

VisitStatementOutput Visitor::visit(CastExpr* expr, const VisitStatementInput& input) {
    return visit_default(expr, input);
}

VisitStatementOutput Visitor::visit(ConditionExpr* expr, const VisitStatementInput& input) {
    return visit_default(expr, input);
}

VisitStatementOutput Visitor::visit(DereferenceExpr* expr, const VisitStatementInput& input) {
    return visit_default(expr, input);
}

VisitStatementOutput Visitor::visit(EntityExpr* expr, const VisitStatementInput& input) {
    return visit_default(expr, input);
}

VisitStatementOutput Visitor::visit(IncDecExpr* expr, const VisitStatementInput& input) {
    return visit_default(expr, input);
}

VisitStatementOutput Visitor::visit(InitializerExpr* expr, const VisitStatementInput& input) {
    return visit_default(expr, input);
}

VisitStatementOutput Visitor::visit(SizeOfExpr* expr, const VisitStatementInput& input) {
    return visit_default(expr, input);
}

VisitStatementOutput Visitor::visit(SubscriptExpr* expr, const VisitStatementInput& input) {
    return visit_default(expr, input);
}

VisitStatementOutput Visitor::visit(UninitializedExpr* expr, const VisitStatementInput& input) {
    return visit_default(expr, input);
}

/* Constants */

VisitStatementOutput Visitor::visit(IntegerConstant* constant, const VisitStatementInput& input) {
    return visit_default(constant, input);
}

VisitStatementOutput Visitor::visit(FloatingPointConstant* constant, const VisitStatementInput& input) {
    return visit_default(constant, input);
}

VisitStatementOutput Visitor::visit(StringConstant* constant, const VisitStatementInput& input) {
    return visit_default(constant, input);
}

