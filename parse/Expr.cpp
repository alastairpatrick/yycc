#include "Expr.h"
#include "Declaration.h"
#include "Message.h"
#include "TranslationUnitContext.h"
#include "visit/Visitor.h"

Expr::Expr(const Location& location): Statement(location) {
}


AddressExpr::AddressExpr(Expr* expr, const Location& location)
    : Expr(location), expr(expr) {
}

VisitStatementOutput AddressExpr::accept(Visitor& visitor, const VisitStatementInput& input) {
    return visitor.visit(this, input);
}

void AddressExpr::print(ostream& stream) const {
    stream << "[\"address\", " << expr << ']';
}


BinaryExpr::BinaryExpr(Expr* left, Expr* right, TokenKind op, const Location& location)
    : Expr(location), left(left), right(right), op(op) {
    assert(this->left);
    assert(this->right);
}

VisitStatementOutput BinaryExpr::accept(Visitor& visitor, const VisitStatementInput& input) {
    return visitor.visit(this, input);
}

void BinaryExpr::print(ostream& stream) const {
    stream << "[\"";

    switch (op) {
      case TOK_LEFT_OP:
        stream << "<<";
        break;
      case TOK_RIGHT_OP:
        stream << ">>";
        break;
      case TOK_LE_OP:
        stream << "<=";
        break;
      case TOK_GE_OP:
        stream << ">=";
        break;
      case TOK_EQ_OP:
        stream << "==";
        break;
      case TOK_NE_OP:
        stream << "!=";
        break;
      case TOK_AND_OP:
        stream << "&&";
        break;
      case TOK_OR_OP:
        stream << "||";
        break;
      case TOK_MUL_ASSIGN:
        stream << "*=";
        break;
      case TOK_DIV_ASSIGN:
        stream << "/=";
        break;
      case TOK_MOD_ASSIGN:
        stream << "%=";
        break;
      case TOK_ADD_ASSIGN:
        stream << "+=";
        break;
      case TOK_SUB_ASSIGN:
        stream << "-=";
        break;
      case TOK_LEFT_ASSIGN:
        stream << "<<=";
        break;
      case TOK_RIGHT_ASSIGN:
        stream << ">>=";
        break;
      case TOK_AND_ASSIGN:
        stream << "&=";
        break;
      case TOK_OR_ASSIGN:
        stream << "|=";
        break;
      case TOK_XOR_ASSIGN:
        stream << "^=";
        break;
      default:
        assert(op > 32 && op < 128);
        stream << (char) op;
        break;
    }

    stream << "\", " << left << ", " << right << "]";
}


CallExpr::CallExpr(Expr* function, vector<Expr*>&& parameters, const Location& location)
    : Expr(location), function(function), parameters(move(parameters)) {
}

VisitStatementOutput CallExpr::accept(Visitor& visitor, const VisitStatementInput& input) {
    return visitor.visit(this, input);
}

void CallExpr::print(ostream& stream) const {
    stream << "[\"call\", " << function;

    for (auto param: parameters) {
        stream  << ", " << param;
    }

    stream << ']';
}


CastExpr::CastExpr(const Type* type, Expr* expr, const Location& location)
    : Expr(location), type(type), expr(expr) {
    assert(type);
    assert(expr);
}

VisitStatementOutput CastExpr::accept(Visitor& visitor, const VisitStatementInput& input) {
    return visitor.visit(this, input);
}

void CastExpr::print(ostream& stream) const {
    stream << "[\"cast\", " << type << ", " << expr << ']';
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


DereferenceExpr::DereferenceExpr(Expr* expr, const Location& location)
    : Expr(location), expr(expr) {
}

VisitStatementOutput DereferenceExpr::accept(Visitor& visitor, const VisitStatementInput& input) {
    return visitor.visit(this, input);
}

void DereferenceExpr::print(ostream& stream) const {
    stream << "[\"deref\", " << expr << ']';
}


EntityExpr::EntityExpr(Declarator* declarator, const Location& location)
    : Expr(location), declarator(declarator) {
}

VisitStatementOutput EntityExpr::accept(Visitor& visitor, const VisitStatementInput& input) {
    return visitor.visit(this, input);
}

void EntityExpr::print(ostream& stream) const {
    stream << "\"N" << declarator->identifier << '"';
}


IncDecExpr::IncDecExpr(TokenKind op, Expr* expr, bool post, const Location& location)
    : Expr(location), op(op), expr(expr), post(post) {
}

VisitStatementOutput IncDecExpr::accept(Visitor& visitor, const VisitStatementInput& input) {
    return visitor.visit(this, input);
}

void IncDecExpr::print(ostream& stream) const {
    stream << "[\"";

    if (post) stream << 'x';

    switch (op) {
      case TOK_INC_OP:
        stream << "++";
        break;
      case TOK_DEC_OP:
        stream << "--";
        break;
    }

    if (!post) stream << 'x';

    stream << "\", " << expr << ']';
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



SizeOfExpr::SizeOfExpr(const Type* type, const Location& location)
    : Expr(location), type(type) {
}

VisitStatementOutput SizeOfExpr::accept(Visitor& visitor, const VisitStatementInput& input) {
    return visitor.visit(this, input);
}

void SizeOfExpr::print(ostream& stream) const {
    stream << "[\"sizeof\", " << type << "]";
}


SubscriptExpr::SubscriptExpr(Expr* left, Expr* right, const Location& location)
    : Expr(location), left(left), right(right) {
}

VisitStatementOutput SubscriptExpr::accept(Visitor& visitor, const VisitStatementInput& input) {
    return visitor.visit(this, input);
}

void SubscriptExpr::print(ostream& stream) const {
    stream << "[\"subs\", " << left << ", " << right << "]";
}

