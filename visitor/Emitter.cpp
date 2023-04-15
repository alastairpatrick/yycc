#include "Emitter.h"
#include "Message.h"
#include "parser/Declaration.h"

Value Emitter::emit(Statement* statement) {
    return statement->accept(*this, VisitStatementInput()).value;
}

Value Emitter::convert_to_type(Value value, const Type* target_type) {
    VisitTypeInput input;
    input.value = value;
    input.target_type = target_type;
    auto result = value.type->accept(*this, input).value;
    assert(result.type == target_type);
    return result;
}

VisitTypeOutput Emitter::visit_default(const Type* type, const VisitTypeInput& input) {
    if (input.target_type == type) return VisitTypeOutput(input.value);

    assert(false);  // TODO
    return VisitTypeOutput(input.value);
}

VisitTypeOutput Emitter::visit(const FloatingPointType* type, const VisitTypeInput& input) {
    auto target_type = input.target_type;
    auto value = input.value;

    if (target_type == type) return VisitTypeOutput(value);

    if (auto float_target = dynamic_cast<const FloatingPointType*>(target_type)) {
        if (float_target->size > type->size) {
            return VisitTypeOutput(target_type, LLVMBuildFPExt(builder, value.llvm, input.target_type->llvm_type(), "cvt"));
        } else {
            return VisitTypeOutput(target_type, LLVMBuildFPTrunc(builder, value.llvm, input.target_type->llvm_type(), "cvt"));
        }
    }

    if (auto int_target = dynamic_cast<const IntegerType*>(input.target_type)) {
        if (int_target->is_signed()) {
            return VisitTypeOutput(target_type, LLVMBuildFPToSI(builder, value.llvm, input.target_type->llvm_type(), "cvt"));
        } else {
            return VisitTypeOutput(target_type, LLVMBuildFPToUI(builder, value.llvm, input.target_type->llvm_type(), "cvt"));
        }
    }

    assert(false);  // TODO
    return VisitTypeOutput(value);
}

VisitTypeOutput Emitter::visit(const IntegerType* type, const VisitTypeInput& input) {
    auto target_type = input.target_type;
    auto value = input.value;

    if (target_type == type) return VisitTypeOutput(value);

    if (auto int_target = dynamic_cast<const IntegerType*>(input.target_type)) {
        if (int_target->size == type->size) return VisitTypeOutput(value);

        if (int_target->size > type->size) {
            if (type->is_signed()) {
                return VisitTypeOutput(target_type, LLVMBuildSExt(builder, value.llvm, input.target_type->llvm_type(), "cvt"));
            } else {
                return VisitTypeOutput(target_type, LLVMBuildZExt(builder, value.llvm, input.target_type->llvm_type(), "cvt"));
            }
        }

        return VisitTypeOutput(target_type, LLVMBuildTrunc(builder, value.llvm, input.target_type->llvm_type(), 0));
    }

    if (auto float_target = dynamic_cast<const FloatingPointType*>(input.target_type)) {
        if (type->is_signed()) {
            return VisitTypeOutput(target_type, LLVMBuildSIToFP(builder, value.llvm, input.target_type->llvm_type(), "cvt"));
        } else {
            return VisitTypeOutput(target_type, LLVMBuildUIToFP(builder, value.llvm, input.target_type->llvm_type(), "cvt"));
        }
    }

    assert(false); // TODO
    return VisitTypeOutput(value);
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

    auto left_temp = convert_to_type(left_value, result_type);
    auto right_temp = convert_to_type(right_value, result_type);

    LLVMValueRef result_value;

    if (auto result_as_int = dynamic_cast<const IntegerType*>(result_type)) {
        switch (expr->op) {
          case BinaryOp::ADD:
            return VisitStatementOutput(result_type, LLVMBuildAdd(builder, left_temp.llvm, right_temp.llvm, "add"));
          case BinaryOp::SUB:
            return VisitStatementOutput(result_type, LLVMBuildSub(builder, left_temp.llvm, right_temp.llvm, "sub"));
          case BinaryOp::MUL:
            return VisitStatementOutput(result_type, LLVMBuildMul(builder, left_temp.llvm, right_temp.llvm, "mul"));
          case BinaryOp::DIV:
            if (result_as_int->is_signed()) {
                return VisitStatementOutput(result_type, LLVMBuildSDiv(builder, left_temp.llvm, right_temp.llvm, "div"));
            } else {
                return VisitStatementOutput(result_type, LLVMBuildUDiv(builder, left_temp.llvm, right_temp.llvm, "div"));
            }
        }
    }

    if (auto result_as_float = dynamic_cast<const FloatingPointType*>(result_type)) {
        switch (expr->op) {
          case BinaryOp::ADD:
            return VisitStatementOutput(result_type, LLVMBuildFAdd(builder, left_temp.llvm, right_temp.llvm, "fadd"));
          case BinaryOp::SUB:
            return VisitStatementOutput(result_type, LLVMBuildFSub(builder, left_temp.llvm, right_temp.llvm, "fsub"));
          case BinaryOp::MUL:
            return VisitStatementOutput(result_type, LLVMBuildFMul(builder, left_temp.llvm, right_temp.llvm, "fmul"));
          case BinaryOp::DIV:
            return VisitStatementOutput(result_type, LLVMBuildFDiv(builder, left_temp.llvm, right_temp.llvm, "fdiv"));
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

    auto cond_value = convert_to_type(condition_value, IntegerType::of(IntegerSignedness::UNSIGNED, IntegerSize::INT));
    LLVMBuildCondBr(builder, cond_value.llvm, alt_blocks[0], alt_blocks[1]);

    LLVMValueRef 
        alt_values[2];
    LLVMPositionBuilderAtEnd(builder, alt_blocks[0]);
    alt_values[0] = convert_to_type(then_value, result_type).llvm;
    LLVMBuildBr(builder, merge_block);

    LLVMPositionBuilderAtEnd(builder, alt_blocks[1]);
    alt_values[1] = convert_to_type(else_value, result_type).llvm;
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

