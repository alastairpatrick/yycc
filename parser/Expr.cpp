#include "Expr.h"
#include "EmitContext.h"
#include "Message.h"
#include "TranslationUnitContext.h"

Value::Value(const Type* type, LLVMValueRef value)
    : value(value), type(type) {
}

bool Value::is_const() const {
    return value && LLVMIsConstant(value);
}

bool Value::is_const_integer() const {
    return value && LLVMIsAConstantInt(value);
}

Expr::Expr(const Location& location): Statement(location) {
}

const Type* Expr::get_type() const {
    auto value = emit(TranslationUnitContext::it->type_emit_context);
    return value.type;
}

Value Expr::fold() const {
    return emit(TranslationUnitContext::it->fold_emit_context);
}

Value Expr::emit(EmitContext& context) const {
    assert(false);
    return Value();
}

ConditionExpr::ConditionExpr(Expr* condition, Expr* then_expr, Expr* else_expr, const Location& location)
    : Expr(location), condition(condition), then_expr(then_expr), else_expr(else_expr) {
    assert(this->condition);
    assert(this->then_expr);
    assert(this->else_expr);
}

Value ConditionExpr::emit(EmitContext& context) const {
    Value condition_value = condition->emit(context);
    Value then_value = then_expr->emit(context);
    Value else_value = else_expr->emit(context);

    auto cond_type = condition_value.type;
    auto then_type = then_value.type;
    auto else_type = else_value.type;
    auto result_type = convert_arithmetic(then_value.type, else_value.type);
    
    if (context.outcome == EmitOutcome::TYPE) return Value(result_type);

    auto builder = context.builder;

    LLVMBasicBlockRef alt_blocks[2] = {
        LLVMAppendBasicBlock(context.function, "then"),
        LLVMAppendBasicBlock(context.function, "else"),
    };
    auto merge_block = LLVMAppendBasicBlock(context.function, "merge");

    auto cond_value = cond_type->convert_to_type(context, condition_value.value, IntegerType::of(IntegerSignedness::UNSIGNED, IntegerSize::INT));
    LLVMBuildCondBr(builder, cond_value, alt_blocks[0], alt_blocks[1]);

    LLVMValueRef 
        alt_values[2];
    LLVMPositionBuilderAtEnd(builder, alt_blocks[0]);
    alt_values[0] = then_type->convert_to_type(context, then_value.value, result_type);
    LLVMBuildBr(builder, merge_block);

    LLVMPositionBuilderAtEnd(builder, alt_blocks[1]);
    alt_values[1] = else_type->convert_to_type(context, else_value.value, result_type);
    LLVMBuildBr(builder, merge_block);

    LLVMPositionBuilderAtEnd(builder, merge_block);
    auto phi_value = LLVMBuildPhi(builder, result_type->llvm_type(), "cond");
    LLVMAddIncoming(phi_value, alt_values, alt_blocks, 2);

    return Value(result_type, phi_value);
}

void ConditionExpr::print(ostream& stream) const {
    stream << "[\"?:\", " << condition << ", " << then_expr << ", " << else_expr << "]";
}

EntityExpr::EntityExpr(Declarator* declarator, const Location& location)
    : Expr(location), declarator(declarator) {
    assert(declarator);
}

void EntityExpr::resolve(ResolveContext& context) {
    declarator->resolve(context);
}

Value EntityExpr::emit(EmitContext& context) const {
    const Type* result_type = declarator->type;

    if (context.outcome == EmitOutcome::TYPE) return Value(result_type);

    // TODO
    assert(false);
    return Value();
}

void EntityExpr::print(ostream& stream) const {
    stream << "\"N" << declarator->identifier << '"';
}

BinaryExpr::BinaryExpr(Expr* left, Expr* right, BinaryOp op, const Location& location)
    : Expr(location), left(left), right(right), op(op) {
    assert(this->left);
    assert(this->right);
}

Value BinaryExpr::emit(EmitContext& context) const {
    auto left_value = left->emit(context);
    auto right_value = right->emit(context);
    auto result_type = convert_arithmetic(left_value.type, right_value.type);

    if (context.outcome == EmitOutcome::TYPE) return Value(result_type);

    auto builder = context.builder;

    auto left_temp = left_value.type->convert_to_type(context, left_value.value, result_type);
    auto right_temp = right_value.type->convert_to_type(context, right_value.value, result_type);

    LLVMValueRef result_value;

    if (auto result_as_int = dynamic_cast<const IntegerType*>(result_type)) {
        switch (op) {
          case BinaryOp::ADD:
            return Value(result_type, LLVMBuildAdd(builder, left_temp, right_temp, "add"));
          case BinaryOp::SUB:
            return Value(result_type, LLVMBuildSub(builder, left_temp, right_temp, "sub"));
          case BinaryOp::MUL:
            return Value(result_type, LLVMBuildMul(builder, left_temp, right_temp, "mul"));
          case BinaryOp::DIV:
            if (result_as_int->is_signed()) {
                return Value(result_type, LLVMBuildSDiv(builder, left_temp, right_temp, "div"));
            } else {
                return Value(result_type, LLVMBuildUDiv(builder, left_temp, right_temp, "div"));
            }
        }
    }

    if (auto result_as_float = dynamic_cast<const FloatingPointType*>(result_type)) {
        switch (op) {
          case BinaryOp::ADD:
            return Value(result_type, LLVMBuildFAdd(builder, left_temp, right_temp, "fadd"));
          case BinaryOp::SUB:
            return Value(result_type, LLVMBuildFSub(builder, left_temp, right_temp, "fsub"));
          case BinaryOp::MUL:
            return Value(result_type, LLVMBuildFMul(builder, left_temp, right_temp, "fmul"));
          case BinaryOp::DIV:
            return Value(result_type, LLVMBuildFDiv(builder, left_temp, right_temp, "fdiv"));
        }
    }

    assert(false); // TODO
    return Value();
}

void BinaryExpr::print(ostream& stream) const {
    switch (op) {
      default:
        stream << "[\"UnknownBinary\", ";
        break;
      case BinaryOp::LOGICAL_OR:
        stream << "[\"||\", ";
        break;
      case BinaryOp::LOGICAL_AND:
        stream << "[\"&&\", ";
        break;
      case BinaryOp::ADD:
        stream << "[\"+\", ";
        break;
      case BinaryOp::SUB:
        stream << "[\"-\", ";
        break;
      case BinaryOp::MUL:
        stream << "[\"*\", ";
        break;
      case BinaryOp::DIV:
        stream << "[\"/\", ";
        break;
      case BinaryOp::MOD:
        stream << "[\"%\", ";
        break;
    }

    stream << left << ", " << right << "]";
}

SizeOfExpr::SizeOfExpr(const Type* type, const Location& location)
    : Expr(location), type(type) {
}

void SizeOfExpr::resolve(ResolveContext& context) {
    type = type->resolve(context);
}

Value SizeOfExpr::emit(EmitContext& context) const {
    auto result_type = IntegerType::uintptr_type();

    if (!type->is_complete()) {
        message(Severity::ERROR, location) << "sizeof applied to incomplete type\n";
        return Value(result_type, LLVMConstInt(result_type->llvm_type(), 0, false));;
    }

    if (context.outcome == EmitOutcome::TYPE) return Value(result_type);

    auto size_int = LLVMStoreSizeOfType(context.target_data, type->llvm_type());
    return Value(result_type, LLVMConstInt(result_type->llvm_type(), size_int, false));
}

void SizeOfExpr::print(ostream& stream) const {
    stream << "[\"sizeof\", " << type << "]";
}

InitializerExpr::InitializerExpr(const Location& location): Expr(location) {
}

void InitializerExpr::print(ostream& stream) const {
    stream << "[\"Init\"";

    for (auto element: elements) {
        stream << ", " << element;
    }

    stream << "]";
}

DefaultExpr::DefaultExpr(const Type* type, const Location& location)
    : Expr(location), type(type) {
}

Value DefaultExpr::emit(EmitContext& context) const {
    // TODO
    return Value(type);
}

void DefaultExpr::print(ostream& stream) const {
    stream << "\"\"";
}
