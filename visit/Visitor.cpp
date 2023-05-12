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

VisitDeclaratorOutput Visitor::visit(Declarator* declarator, TypeDelegate* type_delegate, const VisitDeclaratorInput& input) {
    return VisitDeclaratorOutput();
}

VisitDeclaratorOutput Visitor::visit(Declarator* declarator, EnumConstant* enum_constant, const VisitDeclaratorInput& input) {
    return VisitDeclaratorOutput();
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

VisitStatementOutput Visitor::visit(ExprStatement* statement, const VisitStatementInput& input) {
    accept(statement->expr, VisitExpressionInput()).expr;
    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(ForStatement* statement, const VisitStatementInput& input) {
    visit_declaration(this, statement->declaration);
    accept(statement->initialize, VisitExpressionInput()).expr;
    accept(statement->condition, VisitExpressionInput()).expr;
    accept(statement->iterate, VisitExpressionInput()).expr;
    accept(statement->body, input);

    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(GoToStatement* statement, const VisitStatementInput& input) {
    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(IfElseStatement* statement, const VisitStatementInput& input) {
    accept(statement->condition, VisitExpressionInput()).expr;
    accept(statement->then_statement, input);
    accept(statement->else_statement, input);
    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(ReturnStatement* statement, const VisitStatementInput& input) {
    accept(statement->expr, VisitExpressionInput()).expr;
    return VisitStatementOutput();
}

VisitStatementOutput Visitor::visit(SwitchStatement* statement, const VisitStatementInput& input) {
    accept(statement->expr, VisitExpressionInput()).expr;
    accept(statement->body, input);
    
    for (auto case_expr: statement->cases) {
        accept(case_expr, VisitExpressionInput()).expr;
    }

    return VisitStatementOutput();
}

/* Expressions */

VisitExpressionOutput Visitor::accept(Expr* expr, const VisitExpressionInput& input) {
    if (!expr) return VisitExpressionOutput();
    return expr->accept(*this, VisitExpressionInput());
}

VisitExpressionOutput Visitor::visit(AddressExpr* expr, const VisitExpressionInput& input) {
    expr->expr = accept(expr->expr, input).expr;
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput Visitor::visit(BinaryExpr* expr, const VisitExpressionInput& input) {
    expr->left = accept(expr->left, input).expr;
    expr->right = accept(expr->right, input).expr;
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput Visitor::visit(CallExpr* expr, const VisitExpressionInput& input) {
    expr->function = accept(expr->function, input).expr;
    for (auto parameter: expr->parameters) {
        accept(parameter, input);
    }
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput Visitor::visit(CastExpr* expr, const VisitExpressionInput& input) {
    expr->expr = accept(expr->expr, input).expr;
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput Visitor::visit(ConditionExpr* expr, const VisitExpressionInput& input) {
    expr->condition = accept(expr->condition, input).expr;
    expr->then_expr = accept(expr->then_expr, input).expr;
    expr->else_expr = accept(expr->else_expr, input).expr;
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput Visitor::visit(DereferenceExpr* expr, const VisitExpressionInput& input) {
    expr->expr = accept(expr->expr, input).expr;
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput Visitor::visit(EntityExpr* expr, const VisitExpressionInput& input) {
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput Visitor::visit(IncDecExpr* expr, const VisitExpressionInput& input) {
    expr->expr = accept(expr->expr, input).expr;
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput Visitor::visit(InitializerExpr* expr, const VisitExpressionInput& input) {
    for (auto& element: expr->elements) {
        element = accept(element, input).expr;
    }
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput Visitor::visit(MemberExpr* expr, const VisitExpressionInput& input) {
    expr->object = accept(expr->object, input).expr;
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput Visitor::visit(SizeOfExpr* expr, const VisitExpressionInput& input) {
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput Visitor::visit(SubscriptExpr* expr, const VisitExpressionInput& input) {
    expr->left = accept(expr->left, input).expr;
    expr->right = accept(expr->right, input).expr;
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput Visitor::visit(UninitializedExpr* expr, const VisitExpressionInput& input) {
    return VisitExpressionOutput(expr);
}

/* Constants */

VisitExpressionOutput Visitor::visit(IntegerConstant* constant, const VisitExpressionInput& input) {
    return VisitExpressionOutput(constant);
}

VisitExpressionOutput Visitor::visit(FloatingPointConstant* constant, const VisitExpressionInput& input) {
    return VisitExpressionOutput(constant);
}

VisitExpressionOutput Visitor::visit(StringConstant* constant, const VisitExpressionInput& input) {
    return VisitExpressionOutput(constant);
}
