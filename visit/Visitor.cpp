#include "Visitor.h"
#include "parse/Declaration.h"

/* Declarations */

VisitDeclaratorOutput Visitor::accept(Declarator* declarator, const VisitDeclaratorInput& input) {
    if (!declarator) return VisitDeclaratorOutput();
    pre_visit(declarator);
    return declarator->accept(*this, input);
}

void Visitor::pre_visit(Declarator* declarator) {
}

VisitDeclaratorOutput Visitor::visit(Declarator* declarator, Variable* variable, const VisitDeclaratorInput& input) {
    return VisitDeclaratorOutput();
}

VisitDeclaratorOutput Visitor::visit(Declarator* declarator, Function* function, const VisitDeclaratorInput& input) {
    return VisitDeclaratorOutput();
}

VisitDeclaratorOutput Visitor::visit(Declarator* declarator, TypeDef* type_def, const VisitDeclaratorInput& input) {
    return VisitDeclaratorOutput();
}

VisitDeclaratorOutput Visitor::visit(Declarator* declarator, EnumConstant* enum_constant, const VisitDeclaratorInput& input) {
    return VisitDeclaratorOutput();
}

/* Types */

VisitTypeOutput Visitor::visit(const VoidType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput Visitor::visit(const IntegerType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput Visitor::visit(const FloatingPointType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput Visitor::visit(const PointerType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput Visitor::visit(const QualifiedType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput Visitor::visit(const UnqualifiedType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput Visitor::visit(const FunctionType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput Visitor::visit(const ResolvedArrayType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput Visitor::visit(const StructType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput Visitor::visit(const UnionType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput Visitor::visit(const EnumType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput Visitor::visit(const TypeOfType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput Visitor::visit(const UnboundType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput Visitor::visit(const UnresolvedArrayType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput Visitor::visit(const TypeDefType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

/* Statements */

VisitStatementOutput Visitor::accept(Statement* statement, const VisitStatementInput& input) {
    if (!statement) return VisitStatementOutput();
    pre_visit(statement);
    return statement->accept(*this, input);
}

void Visitor::pre_visit(Statement* statement) {
}

static void visit_declaration(Visitor* visitor, Declaration* declaration) {
    if (!declaration) return;

    for (auto declarator: declaration->declarators) {
        visitor->accept(declarator, VisitDeclaratorInput());
    }
}


VisitStatementOutput Visitor::visit(CompoundStatement* statement, const VisitStatementInput& input) {
    for (auto node: statement->nodes) {
        if (auto declaration = dynamic_cast<Declaration*>(node)) {
            visit_declaration(this, declaration);
        }

        if (auto statement = dynamic_cast<Statement*>(node)) {
            accept(statement, input);
        }
    }
    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(ForStatement* statement, const VisitStatementInput& input) {
    visit_declaration(this, statement->declaration);
    accept(statement->initialize, input);
    accept(statement->condition, input);
    accept(statement->iterate, input);
    accept(statement->body, input);

    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(GoToStatement* statement, const VisitStatementInput& input) {
    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(IfElseStatement* statement, const VisitStatementInput& input) {
    accept(statement->condition, input);
    accept(statement->then_statement, input);
    accept(statement->else_statement, input);
    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(ReturnStatement* statement, const VisitStatementInput& input) {
    accept(statement->expr, input);
    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(SwitchStatement* statement, const VisitStatementInput& input) {
    accept(statement->expr, input);
    accept(statement->body, input);
    
    for (auto case_expr: statement->cases) {
        accept(case_expr, input);
    }

    return VisitStatementOutput();
}

/* Expressions */

VisitStatementOutput Visitor::visit(AddressExpr* expr, const VisitStatementInput& input) {
    accept(expr->expr, input);
    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(BinaryExpr* expr, const VisitStatementInput& input) {
    accept(expr->left, input);
    accept(expr->right, input);
    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(CallExpr* expr, const VisitStatementInput& input) {
    accept(expr->function, input);
    for (auto parameter: expr->parameters) {
        accept(parameter, input);
    }
    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(CastExpr* expr, const VisitStatementInput& input) {
    accept(expr->expr, input);
    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(ConditionExpr* expr, const VisitStatementInput& input) {
    accept(expr->condition, input);
    accept(expr->then_expr, input);
    accept(expr->else_expr, input);
    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(DereferenceExpr* expr, const VisitStatementInput& input) {
    accept(expr->expr, input);
    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(EntityExpr* expr, const VisitStatementInput& input) {
    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(IncDecExpr* expr, const VisitStatementInput& input) {
    accept(expr->expr, input);
    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(InitializerExpr* expr, const VisitStatementInput& input) {
    for (auto element: expr->elements) {
        accept(element, input);
    }
    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(MemberExpr* expr, const VisitStatementInput& input) {
    accept(expr->object, input);
    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(SizeOfExpr* expr, const VisitStatementInput& input) {
    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(SubscriptExpr* expr, const VisitStatementInput& input) {
    accept(expr->left, input);
    accept(expr->right, input);
    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(UninitializedExpr* expr, const VisitStatementInput& input) {
    return VisitStatementOutput();
}

/* Constants */

VisitStatementOutput Visitor::visit(IntegerConstant* constant, const VisitStatementInput& input) {
    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(FloatingPointConstant* constant, const VisitStatementInput& input) {
    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(StringConstant* constant, const VisitStatementInput& input) {
    return VisitStatementOutput();
}
