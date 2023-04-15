#include "Emitter.h"
#include "Message.h"
#include "parser/Declaration.h"

Value Emitter::emit(Statement* statement) {
    return statement->accept(*this, VisitStatementInput()).value;
}

VisitStatementOutput Emitter::visit_default(Expr* expr, const VisitStatementInput& input) {
    assert(false);
    return VisitStatementOutput();
}

VisitStatementOutput Emitter::visit(BinaryExpr* expr, const VisitStatementInput& input) {
    auto left_value = emit(expr->left);
    auto right_value = emit(expr->right);
    auto result_type = convert_arithmetic(left_value.type, right_value.type);

    if (outcome == EmitOutcome::TYPE) return VisitStatementOutput(result_type);

    auto left_temp = left_value.type->convert_to_type(*this, left_value.value, result_type);
    auto right_temp = right_value.type->convert_to_type(*this, right_value.value, result_type);

    LLVMValueRef result_value;

    if (auto result_as_int = dynamic_cast<const IntegerType*>(result_type)) {
        switch (expr->op) {
          case BinaryOp::ADD:
            return VisitStatementOutput(result_type, LLVMBuildAdd(builder, left_temp, right_temp, "add"));
          case BinaryOp::SUB:
            return VisitStatementOutput(result_type, LLVMBuildSub(builder, left_temp, right_temp, "sub"));
          case BinaryOp::MUL:
            return VisitStatementOutput(result_type, LLVMBuildMul(builder, left_temp, right_temp, "mul"));
          case BinaryOp::DIV:
            if (result_as_int->is_signed()) {
                return VisitStatementOutput(result_type, LLVMBuildSDiv(builder, left_temp, right_temp, "div"));
            } else {
                return VisitStatementOutput(result_type, LLVMBuildUDiv(builder, left_temp, right_temp, "div"));
            }
        }
    }

    if (auto result_as_float = dynamic_cast<const FloatingPointType*>(result_type)) {
        switch (expr->op) {
          case BinaryOp::ADD:
            return VisitStatementOutput(result_type, LLVMBuildFAdd(builder, left_temp, right_temp, "fadd"));
          case BinaryOp::SUB:
            return VisitStatementOutput(result_type, LLVMBuildFSub(builder, left_temp, right_temp, "fsub"));
          case BinaryOp::MUL:
            return VisitStatementOutput(result_type, LLVMBuildFMul(builder, left_temp, right_temp, "fmul"));
          case BinaryOp::DIV:
            return VisitStatementOutput(result_type, LLVMBuildFDiv(builder, left_temp, right_temp, "fdiv"));
        }
    }

    assert(false); // TODO
    return VisitStatementOutput();
}

VisitStatementOutput Emitter::visit(ConditionExpr* expr, const VisitStatementInput& input) {
    Value condition_value = emit(expr->condition);
    Value then_value = emit(expr->then_expr);
    Value else_value = emit(expr->else_expr);

    auto cond_type = condition_value.type;
    auto then_type = then_value.type;
    auto else_type = else_value.type;
    auto result_type = convert_arithmetic(then_value.type, else_value.type);
    
    if (outcome == EmitOutcome::TYPE) return VisitStatementOutput(result_type);

    LLVMBasicBlockRef alt_blocks[2] = {
        LLVMAppendBasicBlock(function, "then"),
        LLVMAppendBasicBlock(function, "else"),
    };
    auto merge_block = LLVMAppendBasicBlock(function, "merge");

    auto cond_value = cond_type->convert_to_type(*this, condition_value.value, IntegerType::of(IntegerSignedness::UNSIGNED, IntegerSize::INT));
    LLVMBuildCondBr(builder, cond_value, alt_blocks[0], alt_blocks[1]);

    LLVMValueRef 
        alt_values[2];
    LLVMPositionBuilderAtEnd(builder, alt_blocks[0]);
    alt_values[0] = then_type->convert_to_type(*this, then_value.value, result_type);
    LLVMBuildBr(builder, merge_block);

    LLVMPositionBuilderAtEnd(builder, alt_blocks[1]);
    alt_values[1] = else_type->convert_to_type(*this, else_value.value, result_type);
    LLVMBuildBr(builder, merge_block);

    LLVMPositionBuilderAtEnd(builder, merge_block);
    auto phi_value = LLVMBuildPhi(builder, result_type->llvm_type(), "cond");
    LLVMAddIncoming(phi_value, alt_values, alt_blocks, 2);

    return VisitStatementOutput(result_type, phi_value);
}

VisitStatementOutput Emitter::visit(EntityExpr* expr, const VisitStatementInput& input) {
    const Type* result_type = expr->declarator->type;

    if (outcome == EmitOutcome::TYPE) return VisitStatementOutput(result_type);

    if (auto enum_constant = dynamic_cast<EnumConstant*>(expr->declarator->delegate)) {
        return VisitStatementOutput(result_type, LLVMConstInt(result_type->llvm_type(), enum_constant->constant_int, true));
    }

    // TODO
    assert(false);
    return VisitStatementOutput();
}

VisitStatementOutput Emitter::visit(SizeOfExpr* expr, const VisitStatementInput& input) {
    auto result_type = IntegerType::uintptr_type();

    if (!expr->type->is_complete()) {
        message(Severity::ERROR, expr->location) << "sizeof applied to incomplete type\n";
        return VisitStatementOutput(result_type, LLVMConstInt(result_type->llvm_type(), 0, false));;
    }

    if (outcome == EmitOutcome::TYPE) return VisitStatementOutput(result_type);

    auto size_int = LLVMStoreSizeOfType(target_data, expr->type->llvm_type());
    return VisitStatementOutput(result_type, LLVMConstInt(result_type->llvm_type(), size_int, false));
}

VisitStatementOutput Emitter::visit(IntegerConstant* constant, const VisitStatementInput& input) {
    return VisitStatementOutput(constant->type, constant->value);
}

VisitStatementOutput Emitter::visit(FloatingPointConstant* constant, const VisitStatementInput& input) {
    return VisitStatementOutput(constant->type, constant->value);
}

