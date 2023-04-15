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

    LLVMTargetRef target{};
    LLVMTargetMachineRef target_machine{};
    LLVMTargetDataRef target_data{};

    LLVMModuleRef mod{};
    LLVMValueRef function{};
    LLVMBuilderRef builder{};

    const Type* get_type(Expr* expr);
    Value emit(Statement* statement);

    virtual VisitStatementOutput visit_default(Expr* expr, const VisitStatementInput& input) override;
    virtual VisitStatementOutput visit(BinaryExpr* expr, const VisitStatementInput& input) override;
    virtual VisitStatementOutput visit(ConditionExpr* expr, const VisitStatementInput& input) override;
    virtual VisitStatementOutput visit(EntityExpr* expr, const VisitStatementInput& input) override;
    virtual VisitStatementOutput visit(SizeOfExpr* expr, const VisitStatementInput& input) override;

    virtual VisitStatementOutput visit(IntegerConstant* constant, const VisitStatementInput& input) override;
    virtual VisitStatementOutput visit(FloatingPointConstant* constant, const VisitStatementInput& input) override;
};

#endif