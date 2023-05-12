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

VisitStatementOutput Visitor::accept(Statement* statement) {
    if (!statement) return VisitStatementOutput();
    pre_visit(statement);
    return statement->accept(*this);
}

void Visitor::pre_visit(Statement* statement) {
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
VisitExpressionOutput Visitor::accept(Expr* expr) {
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




/* Declarations */

VisitDeclaratorOutput DepthFirstVisitor::visit(Declarator* declarator, Variable* variable, const VisitDeclaratorInput& input) {
    return VisitDeclaratorOutput();
}

VisitDeclaratorOutput DepthFirstVisitor::visit(Declarator* declarator, Function* function, const VisitDeclaratorInput& input) {
    return VisitDeclaratorOutput();
}

VisitDeclaratorOutput DepthFirstVisitor::visit(Declarator* declarator, TypeDelegate* type_delegate, const VisitDeclaratorInput& input) {
    return VisitDeclaratorOutput();
}

VisitDeclaratorOutput DepthFirstVisitor::visit(Declarator* declarator, EnumConstant* enum_constant, const VisitDeclaratorInput& input) {
    return VisitDeclaratorOutput();
}

/* Statements */

static void visit_declaration(DepthFirstVisitor* visitor, Declaration* declaration) {
    if (!declaration) return;

    for (auto declarator: declaration->declarators) {
        visitor->accept(declarator, VisitDeclaratorInput());
    }
}


VisitStatementOutput DepthFirstVisitor::visit(CompoundStatement* statement) {
    for (auto node: statement->nodes) {
        if (auto declaration = dynamic_cast<Declaration*>(node)) {
            visit_declaration(this, declaration);
        }

        if (auto statement = dynamic_cast<Statement*>(node)) {
            accept(statement);
        }
    }
    return VisitStatementOutput();
}

VisitStatementOutput DepthFirstVisitor::visit(ExprStatement* statement) {
    statement->expr = accept(statement->expr).expr;
    return VisitStatementOutput();
}

VisitStatementOutput DepthFirstVisitor::visit(ForStatement* statement) {
    visit_declaration(this, statement->declaration);
    statement->initialize = accept(statement->initialize).expr;
    statement->condition = accept(statement->condition).expr;
    statement->iterate = accept(statement->iterate).expr;
    accept(statement->body);

    return VisitStatementOutput();
}

VisitStatementOutput DepthFirstVisitor::visit(GoToStatement* statement) {
    return VisitStatementOutput();
}

VisitStatementOutput DepthFirstVisitor::visit(IfElseStatement* statement) {
    statement->condition = accept(statement->condition).expr;
    accept(statement->then_statement);
    accept(statement->else_statement);
    return VisitStatementOutput();
}

VisitStatementOutput DepthFirstVisitor::visit(ReturnStatement* statement) {
    statement->expr = accept(statement->expr).expr;
    return VisitStatementOutput();
}

VisitStatementOutput DepthFirstVisitor::visit(SwitchStatement* statement) {
    statement->expr = accept(statement->expr).expr;
    accept(statement->body);
    
    for (auto& case_expr: statement->cases) {
        case_expr = accept(case_expr).expr;
    }

    return VisitStatementOutput();
}

/* Expressions */

VisitExpressionOutput DepthFirstVisitor::visit(AddressExpr* expr) {
    expr->expr = accept(expr->expr).expr;
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput DepthFirstVisitor::visit(BinaryExpr* expr) {
    expr->left = accept(expr->left).expr;
    expr->right = accept(expr->right).expr;
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput DepthFirstVisitor::visit(CallExpr* expr) {
    expr->function = accept(expr->function).expr;
    for (auto& parameter: expr->parameters) {
        parameter = accept(parameter).expr;
    }
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput DepthFirstVisitor::visit(CastExpr* expr) {
    expr->expr = accept(expr->expr).expr;
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput DepthFirstVisitor::visit(ConditionExpr* expr) {
    expr->condition = accept(expr->condition).expr;
    expr->then_expr = accept(expr->then_expr).expr;
    expr->else_expr = accept(expr->else_expr).expr;
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput DepthFirstVisitor::visit(DereferenceExpr* expr) {
    expr->expr = accept(expr->expr).expr;
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput DepthFirstVisitor::visit(EntityExpr* expr) {
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput DepthFirstVisitor::visit(IncDecExpr* expr) {
    expr->expr = accept(expr->expr).expr;
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput DepthFirstVisitor::visit(InitializerExpr* expr) {
    for (auto& element: expr->elements) {
        element = accept(element).expr;
    }
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput DepthFirstVisitor::visit(MemberExpr* expr) {
    expr->object = accept(expr->object).expr;
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput DepthFirstVisitor::visit(SizeOfExpr* expr) {
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput DepthFirstVisitor::visit(SubscriptExpr* expr) {
    expr->left = accept(expr->left).expr;
    expr->right = accept(expr->right).expr;
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput DepthFirstVisitor::visit(UninitializedExpr* expr) {
    return VisitExpressionOutput(expr);
}

/* Constants */

VisitExpressionOutput DepthFirstVisitor::visit(IntegerConstant* constant) {
    return VisitExpressionOutput(constant);
}

VisitExpressionOutput DepthFirstVisitor::visit(FloatingPointConstant* constant) {
    return VisitExpressionOutput(constant);
}

VisitExpressionOutput DepthFirstVisitor::visit(StringConstant* constant) {
    return VisitExpressionOutput(constant);
}
