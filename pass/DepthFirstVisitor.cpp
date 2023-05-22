#include "DepthFirstVisitor.h"

#include "parse/Declaration.h"

/* Declarations */

VisitDeclaratorOutput DepthFirstVisitor::visit(Declarator* declarator, Variable* variable, const VisitDeclaratorInput& input) {
    if (variable->member && variable->member->bit_field) {
        variable->member->bit_field->expr = accept_expr(variable->member->bit_field->expr).expr;
    }

    variable->initializer = accept_expr(variable->initializer).expr;

    return VisitDeclaratorOutput();
}

VisitDeclaratorOutput DepthFirstVisitor::visit(Declarator* declarator, Function* function, const VisitDeclaratorInput& input) {
    auto function_type = dynamic_cast<const FunctionType*>(declarator->type);
    for (size_t i = 0; i < function->parameters.size(); ++i) {
        accept_declarator(function->parameters[i]);
    }

    if (function->body) {
        accept_statement(function->body);
    }

    return VisitDeclaratorOutput();
}

VisitDeclaratorOutput DepthFirstVisitor::visit(Declarator* declarator, TypeDelegate* type_delegate, const VisitDeclaratorInput& input) {
    return VisitDeclaratorOutput();
}

VisitDeclaratorOutput DepthFirstVisitor::visit(Declarator* declarator, EnumConstant* enum_constant, const VisitDeclaratorInput& input) {
    return VisitDeclaratorOutput();
}

/* Statements */

// todo: rename to accept_declaration
static void visit_declaration(DepthFirstVisitor* visitor, Declaration* declaration) {
    if (!declaration) return;

    for (auto declarator: declaration->declarators) {
        visitor->accept_declarator(declarator);
    }
}

VisitStatementOutput DepthFirstVisitor::accept_statement(Statement* statement) {
    if (!statement) return VisitStatementOutput();

    for (auto& label: statement->labels) {
        label.case_expr = accept_expr(label.case_expr).expr;
    }

    return statement->accept(*this);
}

VisitStatementOutput DepthFirstVisitor::visit(CompoundStatement* statement) {
    for (auto& node: statement->nodes) {
        if (auto declaration = dynamic_cast<Declaration*>(node)) {
            visit_declaration(this, declaration);
        }

        if (auto statement = dynamic_cast<Statement*>(node)) {
            node = accept_statement(statement).statement;
        }
    }
    return VisitStatementOutput(statement);
}

VisitStatementOutput DepthFirstVisitor::visit(ExprStatement* statement) {
    statement->expr = accept_expr(statement->expr).expr;
    return VisitStatementOutput(statement);
}

VisitStatementOutput DepthFirstVisitor::visit(ForStatement* statement) {
    visit_declaration(this, statement->declaration);
    statement->initialize = accept_expr(statement->initialize).expr;
    statement->condition = accept_expr(statement->condition).expr;
    statement->iterate = accept_expr(statement->iterate).expr;
    statement->body = accept_statement(statement->body).statement;

    return VisitStatementOutput(statement);
}

VisitStatementOutput DepthFirstVisitor::visit(JumpStatement* statement) {
    return VisitStatementOutput(statement);
}

VisitStatementOutput DepthFirstVisitor::visit(IfElseStatement* statement) {
    statement->condition = accept_expr(statement->condition).expr;
    statement->then_statement = accept_statement(statement->then_statement).statement;
    statement->else_statement = accept_statement(statement->else_statement).statement;
    return VisitStatementOutput(statement);
}

VisitStatementOutput DepthFirstVisitor::visit(ReturnStatement* statement) {
    statement->expr = accept_expr(statement->expr).expr;
    return VisitStatementOutput(statement);
}

VisitStatementOutput DepthFirstVisitor::visit(SwitchStatement* statement) {
    statement->expr = accept_expr(statement->expr).expr;
    statement->body = accept_statement(statement->body).statement;
    
    for (auto& case_expr: statement->cases) {
        case_expr = accept_expr(case_expr).expr;
    }

    return VisitStatementOutput(statement);
}

/* Expressions */

VisitExpressionOutput DepthFirstVisitor::visit(AddressExpr* expr) {
    expr->expr = accept_expr(expr->expr).expr;
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput DepthFirstVisitor::visit(AssignExpr* expr) {
    expr->left = accept_expr(expr->left).expr;
    expr->right = accept_expr(expr->right).expr;
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput DepthFirstVisitor::visit(BinaryExpr* expr) {
    expr->left = accept_expr(expr->left).expr;
    expr->right = accept_expr(expr->right).expr;
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput DepthFirstVisitor::visit(CallExpr* expr) {
    expr->function = accept_expr(expr->function).expr;
    for (auto& parameter: expr->parameters) {
        parameter = accept_expr(parameter).expr;
    }
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput DepthFirstVisitor::visit(CastExpr* expr) {
    expr->expr = accept_expr(expr->expr).expr;
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput DepthFirstVisitor::visit(ConditionExpr* expr) {
    expr->condition = accept_expr(expr->condition).expr;
    expr->then_expr = accept_expr(expr->then_expr).expr;
    expr->else_expr = accept_expr(expr->else_expr).expr;
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput DepthFirstVisitor::visit(DereferenceExpr* expr) {
    expr->expr = accept_expr(expr->expr).expr;
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput DepthFirstVisitor::visit(EntityExpr* expr) {
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput DepthFirstVisitor::visit(IncDecExpr* expr) {
    expr->expr = accept_expr(expr->expr).expr;
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput DepthFirstVisitor::visit(InitializerExpr* expr) {
    for (auto& element: expr->elements) {
        element = accept_expr(element).expr;
    }
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput DepthFirstVisitor::visit(MemberExpr* expr) {
    expr->object = accept_expr(expr->object).expr;
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput DepthFirstVisitor::visit(MoveExpr* expr) {
    expr->expr = accept_expr(expr->expr).expr;
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput DepthFirstVisitor::visit(SizeOfExpr* expr) {
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput DepthFirstVisitor::visit(SequenceExpr* expr) {
    expr->left = accept_expr(expr->left).expr;
    expr->right = accept_expr(expr->right).expr;
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput DepthFirstVisitor::visit(SubscriptExpr* expr) {
    expr->left = accept_expr(expr->left).expr;
    expr->right = accept_expr(expr->right).expr;
    return VisitExpressionOutput(expr);
}

VisitExpressionOutput DepthFirstVisitor::visit(UnaryExpr* expr) {
    expr->expr = accept_expr(expr->expr).expr;
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
