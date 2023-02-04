#include "nlohmann/json.hpp"

#include "std.h"

#include "Expr.h"
#include "CodeGenContext.h"

using json = nlohmann::json;

ConditionExpr::ConditionExpr(shared_ptr<Expr> condition, shared_ptr<Expr> then_expr, shared_ptr<Expr> else_expr, const Location& location)
    : Expr(location), condition(move(condition)), then_expr(move(then_expr)), else_expr(move(else_expr)) {
    assert(this->condition);
    assert(this->then_expr);
    assert(this->else_expr);
}

const Type* ConditionExpr::get_type() const {
    return convert_arithmetic(then_expr->get_type(), else_expr->get_type());
}

LLVMValueRef ConditionExpr::codegen(CodeGenContext* context) const {
    auto builder = context->builder;

    auto cond_type = condition->get_type();
    auto then_type = then_expr->get_type();
    auto else_type = else_expr->get_type();
    auto result_type = get_type();

    LLVMBasicBlockRef alt_blocks[2] = {
        LLVMAppendBasicBlock(context->function, "then"),
        LLVMAppendBasicBlock(context->function, "else"),
    };
    auto merge_block = LLVMAppendBasicBlock(context->function, "merge");

    auto cond_value = cond_type->convert_to_type(context, condition->codegen(context), IntegerType::of(IntegerSignedness::UNSIGNED, IntegerSize::INT));
    LLVMBuildCondBr(builder, cond_value, alt_blocks[0], alt_blocks[1]);

    LLVMValueRef 
        alt_values[2];
    LLVMPositionBuilderAtEnd(builder, alt_blocks[0]);
    alt_values[0] = then_type->convert_to_type(context, then_expr->codegen(context), result_type);
    LLVMBuildBr(builder, merge_block);

    LLVMPositionBuilderAtEnd(builder, alt_blocks[1]);
    alt_values[1] = else_type->convert_to_type(context, else_expr->codegen(context), result_type);
    LLVMBuildBr(builder, merge_block);

    LLVMPositionBuilderAtEnd(builder, merge_block);
    auto phi_value = LLVMBuildPhi(builder, result_type->llvm_type(), "cond");
    LLVMAddIncoming(phi_value, alt_values, alt_blocks, 2);

    return phi_value;
}

void ConditionExpr::print(ostream& stream) const {
    stream << "[\"?:\", " << condition << ", " << then_expr << ", " << else_expr << "]";
}

Constant::Constant(const Location& location): Expr(location) {
}

IntegerConstant::IntegerConstant(unsigned long long value, const IntegerType* type, const Location& location)
    : Constant(location), type(type), value(value) {
}

const Type* IntegerConstant::get_type() const {
    return type;
}

LLVMValueRef IntegerConstant::codegen(CodeGenContext* context) const {
    return LLVMConstInt(type->llvm_type(), value, type->is_signed());
}

void IntegerConstant::print(ostream& stream) const {
    stream << '"' << type << value << '"';
}

FloatingPointConstant::FloatingPointConstant(double value, const FloatingPointType* type, const Location& location)
    : Constant(location), type(type), value(value) {
}

const Type* FloatingPointConstant::get_type() const {
    return type;
}

LLVMValueRef FloatingPointConstant::codegen(CodeGenContext* context) const {
    return LLVMConstReal(type->llvm_type(), value);
}

void FloatingPointConstant::print(ostream& stream) const {
    stream << '"' << type << value << '"';
}

StringConstant::StringConstant(std::string utf8_literal, const IntegerType* char_type, const Location& location)
    : Constant(location), char_type(char_type), utf8_literal(move(utf8_literal)) {
}

const Type* StringConstant::get_type() const {
    return char_type->pointer_to();
}

void StringConstant::print(ostream& stream) const {
    stringstream s;
    s << 'S' << char_type << utf8_literal;
    string t(s.str());
    stream << json(t);
}

LLVMValueRef StringConstant::codegen(CodeGenContext* context) const {
    return nullptr;
}

NameExpr::NameExpr(string name, const Location& location)
    : Expr(location), name(move(name)) {
}

const Type* NameExpr::get_type() const {
    assert(false);
    return nullptr;
}

LLVMValueRef NameExpr::codegen(CodeGenContext* context) const {
    assert(false);
    return nullptr;
}

void NameExpr::print(std::ostream& stream) const {
    stream << "\"N" << name << '"';
}

BinaryExpr::BinaryExpr(shared_ptr<Expr> left, shared_ptr<Expr> right, BinaryOp op, const Location& location)
    : Expr(location), left(move(left)), right(move(right)), op(op) {
    assert(this->left);
    assert(this->right);
}

const Type* BinaryExpr::get_type() const {
    return convert_arithmetic(left->get_type(), right->get_type());
}

LLVMValueRef BinaryExpr::codegen(CodeGenContext* context) const {
    auto builder = context->builder;

    auto left_type = left->get_type();
    auto right_type = right->get_type();
    auto result_type = get_type();

    auto left_temp = left_type->convert_to_type(context, left->codegen(context), result_type);
    auto right_temp = right_type->convert_to_type(context, right->codegen(context), result_type);

    if (auto result_as_int = dynamic_cast<const IntegerType*>(result_type)) {
        switch (op) {
        case BinaryOp::ADD:
            return LLVMBuildAdd(builder, left_temp, right_temp, "add");
        case BinaryOp::SUB:
            return LLVMBuildSub(builder, left_temp, right_temp, "sub");
        case BinaryOp::MUL:
            return LLVMBuildMul(builder, left_temp, right_temp, "mul");
        case BinaryOp::DIV:
            if (result_as_int->is_signed()) {
                return LLVMBuildSDiv(builder, left_temp, right_temp, "div");
            } else {
                return LLVMBuildUDiv(builder, left_temp, right_temp, "div");
            }
        }
    }

    if (auto result_as_float = dynamic_cast<const FloatingPointType*>(result_type)) {
        switch (op) {
        case BinaryOp::ADD:
            return LLVMBuildFAdd(builder, left_temp, right_temp, "fadd");
        case BinaryOp::SUB:
            return LLVMBuildFSub(builder, left_temp, right_temp, "fsub");
        case BinaryOp::MUL:
            return LLVMBuildFMul(builder, left_temp, right_temp, "fmul");
        case BinaryOp::DIV:
            return LLVMBuildFDiv(builder, left_temp, right_temp, "fdiv");
        }
    }

    assert(false); // TODO
    return nullptr;
}

void BinaryExpr::print(ostream& stream) const {
    switch (op) {
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
    default:
        stream << "[\"UnknownBinary\", ";
        break;
    }

    stream << left << ", " << right << "]";
}
