#include "Expr.h"
#include "Declaration.h"
#include "Message.h"
#include "TranslationUnitContext.h"
#include "visitor/Visitor.h"
#include "visitor/Emitter.h"

Value::Value(const Type* type, LLVMValueRef llvm)
    : llvm(llvm), type(type) {
}

bool Value::is_const() const {
    return llvm && LLVMIsConstant(llvm);
}

bool Value::is_const_integer() const {
    return llvm && LLVMIsAConstantInt(llvm);
}

Expr::Expr(const Location& location): Statement(location) {
}

const Type* Expr::get_type() const {
    auto& emitter = TranslationUnitContext::it->type_emitter;
    return emitter.emit(const_cast<Expr*>(this)).type;
}

Value Expr::fold() const {
    auto& emitter = TranslationUnitContext::it->fold_emitter;
    return emitter.emit(const_cast<Expr*>(this));
}

ConditionExpr::ConditionExpr(Expr* condition, Expr* then_expr, Expr* else_expr, const Location& location)
    : Expr(location), condition(condition), then_expr(then_expr), else_expr(else_expr) {
    assert(this->condition);
    assert(this->then_expr);
    assert(this->else_expr);
}

VisitStatementOutput ConditionExpr::accept(Visitor& visitor, const VisitStatementInput& input) {
    return visitor.visit(this, input);
}

void ConditionExpr::print(ostream& stream) const {
    stream << "[\"?:\", " << condition << ", " << then_expr << ", " << else_expr << "]";
}

EntityExpr::EntityExpr(Declarator* declarator, const Location& location)
    : Expr(location), declarator(declarator) {
    assert(declarator);
}

VisitStatementOutput EntityExpr::accept(Visitor& visitor, const VisitStatementInput& input) {
    return visitor.visit(this, input);
}

void EntityExpr::print(ostream& stream) const {
    stream << "\"N" << declarator->identifier << '"';
}

BinaryExpr::BinaryExpr(Expr* left, Expr* right, BinaryOp op, const Location& location)
    : Expr(location), left(left), right(right), op(op) {
    assert(this->left);
    assert(this->right);
}

VisitStatementOutput BinaryExpr::accept(Visitor& visitor, const VisitStatementInput& input) {
    return visitor.visit(this, input);
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

VisitStatementOutput SizeOfExpr::accept(Visitor& visitor, const VisitStatementInput& input) {
    return visitor.visit(this, input);
}

void SizeOfExpr::print(ostream& stream) const {
    stream << "[\"sizeof\", " << type << "]";
}

InitializerExpr::InitializerExpr(const Location& location): Expr(location) {
}

VisitStatementOutput InitializerExpr::accept(Visitor& visitor, const VisitStatementInput& input) {
    return visitor.visit(this, input);
}

void InitializerExpr::print(ostream& stream) const {
    stream << "[\"Init\"";

    for (auto element: elements) {
        stream << ", " << element;
    }

    stream << "]";
}
