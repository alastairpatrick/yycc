#ifndef VISITOR_EMITTER_H
#define VISITOR_EMITTER_H

#include "Visitor.h"

enum class EmitOutcome {
    TYPE,
    FOLD,
    IR,
};

struct Emitter: Visitor {
    EmitOutcome outcome = EmitOutcome::TYPE;

    LLVMModuleRef module{};
    LLVMValueRef function{};
    LLVMBuilderRef builder{};

    void emit(ASTNode* node);
    void emit(Declaration* declaration);
    void emit(Declarator* declarator);
    Value emit(Statement* statement);

    const Type* get_type(Expr* expr);
    Value convert_to_type(Value value, const Type* target_type);

    virtual VisitDeclaratorOutput visit(Declarator* declarator, Entity* entity, const VisitDeclaratorInput& input) override;

    virtual VisitTypeOutput visit_default(const Type* type, const VisitTypeInput& input) override;
    virtual VisitTypeOutput visit(const FloatingPointType* type, const VisitTypeInput& input) override;
    virtual VisitTypeOutput visit(const IntegerType* type, const VisitTypeInput& input) override;

    virtual VisitStatementOutput visit(CompoundStatement* statement, const VisitStatementInput& input) override;
    virtual VisitStatementOutput visit(ReturnStatement* statement, const VisitStatementInput& input) override;

    virtual VisitStatementOutput visit_default(Expr* expr, const VisitStatementInput& input) override;
    virtual VisitStatementOutput visit(BinaryExpr* expr, const VisitStatementInput& input) override;
    virtual VisitStatementOutput visit(ConditionExpr* expr, const VisitStatementInput& input) override;
    virtual VisitStatementOutput visit(EntityExpr* expr, const VisitStatementInput& input) override;
    virtual VisitStatementOutput visit(SizeOfExpr* expr, const VisitStatementInput& input) override;

    virtual VisitStatementOutput visit(IntegerConstant* constant, const VisitStatementInput& input) override;
    virtual VisitStatementOutput visit(FloatingPointConstant* constant, const VisitStatementInput& input) override;
};

#endif