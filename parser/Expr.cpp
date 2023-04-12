#include "Expr.h"
#include "CodeGenContext.h"

Value::Value(LLVMValueRef value, const Type* type)
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

Value Expr::evaluate_constant() const {
    return Value();
}

Value Expr::generate_value(CodeGenContext* context) const {
    assert(false);
    return Value();
}

ConditionExpr::ConditionExpr(Expr* condition, Expr* then_expr, Expr* else_expr, const Location& location)
    : Expr(location), condition(condition), then_expr(then_expr), else_expr(else_expr) {
    assert(this->condition);
    assert(this->then_expr);
    assert(this->else_expr);
}

Value ConditionExpr::generate_value(CodeGenContext* context) const {
    Value condition_value = condition->generate_value(context);
    Value then_value = then_expr->generate_value(context);
    Value else_value = else_expr->generate_value(context);

    auto cond_type = condition_value.type;
    auto then_type = then_value.type;
    auto else_type = else_value.type;
    auto result_type = convert_arithmetic(then_value.type, else_value.type);
    
    return Value(nullptr, result_type);

    auto builder = context->builder;

    LLVMBasicBlockRef alt_blocks[2] = {
        LLVMAppendBasicBlock(context->function, "then"),
        LLVMAppendBasicBlock(context->function, "else"),
    };
    auto merge_block = LLVMAppendBasicBlock(context->function, "merge");

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

    return Value(phi_value, result_type);
}

void ConditionExpr::print(ostream& stream) const {
    stream << "[\"?:\", " << condition << ", " << then_expr << ", " << else_expr << "]";
}

NameExpr::NameExpr(const Declarator* declarator, const Location& location)
    : Expr(location), declarator(declarator) {
    assert(declarator);
}

void NameExpr::print(ostream& stream) const {
    stream << "\"N" << declarator->identifier << '"';
}

BinaryExpr::BinaryExpr(Expr* left, Expr* right, BinaryOp op, const Location& location)
    : Expr(location), left(left), right(right), op(op) {
    assert(this->left);
    assert(this->right);
}

Value BinaryExpr::generate_value(CodeGenContext* context) const {
    auto left_value = left->generate_value(context);
    auto right_value = right->generate_value(context);
    auto result_type = convert_arithmetic(left_value.type, right_value.type);

    return Value(nullptr, result_type);

    auto builder = context->builder;

    auto left_temp = left_value.type->convert_to_type(context, left_value.value, result_type);
    auto right_temp = right_value.type->convert_to_type(context, right_value.value, result_type);

    LLVMValueRef result_value;

    if (auto result_as_int = dynamic_cast<const IntegerType*>(result_type)) {
        switch (op) {
          case BinaryOp::ADD:
            return Value(LLVMBuildAdd(builder, left_temp, right_temp, "add"), result_type);
          case BinaryOp::SUB:
            return Value(LLVMBuildSub(builder, left_temp, right_temp, "sub"), result_type);
          case BinaryOp::MUL:
            return Value(LLVMBuildMul(builder, left_temp, right_temp, "mul"), result_type);
          case BinaryOp::DIV:
            if (result_as_int->is_signed()) {
                return Value(LLVMBuildSDiv(builder, left_temp, right_temp, "div"), result_type);
            } else {
                return Value(LLVMBuildUDiv(builder, left_temp, right_temp, "div"), result_type);
            }
        }
    }

    if (auto result_as_float = dynamic_cast<const FloatingPointType*>(result_type)) {
        switch (op) {
          case BinaryOp::ADD:
            return Value(LLVMBuildFAdd(builder, left_temp, right_temp, "fadd"), result_type);
          case BinaryOp::SUB:
            return Value(LLVMBuildFSub(builder, left_temp, right_temp, "fsub"), result_type);
          case BinaryOp::MUL:
            return Value(LLVMBuildFMul(builder, left_temp, right_temp, "fmul"), result_type);
          case BinaryOp::DIV:
            return Value(LLVMBuildFDiv(builder, left_temp, right_temp, "fdiv"), result_type);
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

DefaultExpr::DefaultExpr(const Type* type, const Location& location)
    : Expr(location), type(type) {
}

Value DefaultExpr::generate_value(CodeGenContext* context) const {
    // TODO
    return Value(nullptr, type);
}

void DefaultExpr::print(ostream& stream) const {
    stream << "\"\"";
}
