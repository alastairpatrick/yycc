#include "Expr.h"
#include "Declaration.h"
#include "Message.h"
#include "pass/Visitor.h"
#include "TranslationUnitContext.h"

Expr::Expr(const Location& location): LocationNode(location) {
}

AddressExpr::AddressExpr(Expr* expr, const Location& location)
    : Expr(location), expr(expr) {
}

VisitExpressionOutput AddressExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

void AddressExpr::print(ostream& stream) const {
    stream << "[\"address\", " << expr << ']';
}

AssignExpr::AssignExpr(Expr* left, Expr* right, const Location& location)
    : Expr(location), left(left), right(right) {
}

VisitExpressionOutput AssignExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

void AssignExpr::print(ostream& stream) const {
    stream << "[\"=\", " << left << ", " << right << "]";
}


BinaryExpr::BinaryExpr(Expr* left, Expr* right, TokenKind op, const Location& location)
    : Expr(location), left(left), right(right), op(op) {
    assert(this->left);
    assert(this->right);
}

string BinaryExpr::message_kind() const {
    switch (op) {
      case TOK_LEFT_OP:
        return "<<";
      case TOK_RIGHT_OP:
        return ">>";
      case TOK_LE_OP:
        return "<=";
      case TOK_GE_OP:
        return ">=";
      case TOK_EQ_OP:
        return "==";
      case TOK_NE_OP:
        return "!=";
      case TOK_AND_OP:
        return "&&";
      case TOK_OR_OP:
        return "||";
      case TOK_MUL_ASSIGN:
        return "*=";
      case TOK_DIV_ASSIGN:
        return "/=";
      case TOK_MOD_ASSIGN:
        return "%=";
      case TOK_ADD_ASSIGN:
        return "+=";
      case TOK_SUB_ASSIGN:
        return "-=";
      case TOK_LEFT_ASSIGN:
        return "<<=";
      case TOK_RIGHT_ASSIGN:
        return ">>=";
      case TOK_AND_ASSIGN:
        return "&=";
      case TOK_OR_ASSIGN:
        return "|=";
      case TOK_XOR_ASSIGN:
        return "^=";
      default:
        assert(op > 32 && op < 128);
        return string(1, (char) op);
    }
}

VisitExpressionOutput BinaryExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

void BinaryExpr::print(ostream& stream) const {
    stream << "[\"" << message_kind() << "\", " << left << ", " << right << "]";
}


CallExpr::CallExpr(Expr* function, vector<Expr*>&& parameters, const Location& location)
    : Expr(location), function(function), parameters(move(parameters)) {
}

VisitExpressionOutput CallExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
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

VisitExpressionOutput CastExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
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

VisitExpressionOutput ConditionExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

void ConditionExpr::print(ostream& stream) const {
    stream << "[\"?:\", " << condition << ", " << then_expr << ", " << else_expr << "]";
}


DereferenceExpr::DereferenceExpr(Expr* expr, const Location& location)
    : Expr(location), expr(expr) {
}

VisitExpressionOutput DereferenceExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

void DereferenceExpr::print(ostream& stream) const {
    stream << "[\"deref\", " << expr << ']';
}


EntityExpr::EntityExpr(Scope* scope, const Identifier& identifier, const Location& location)
    : Expr(location), scope(scope), identifier(identifier) {
}

VisitExpressionOutput EntityExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

void EntityExpr::print(ostream& stream) const {
    stream << "\"N" << *identifier.text << '"';
}


IncDecExpr::IncDecExpr(TokenKind op, Expr* expr, bool postfix, const Location& location)
    : Expr(location), op(op), expr(expr), postfix(postfix) {
}

VisitExpressionOutput IncDecExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

void IncDecExpr::print(ostream& stream) const {
    stream << "[\"";

    if (postfix) stream << 'x';

    switch (op) {
      case TOK_INC_OP:
        stream << "++";
        break;
      case TOK_DEC_OP:
        stream << "--";
        break;
    }

    if (!postfix) stream << 'x';

    stream << "\", " << expr << ']';
}


InitializerExpr::InitializerExpr(const Location& location): Expr(location) {
}

VisitExpressionOutput InitializerExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

void InitializerExpr::print(ostream& stream) const {
    stream << "[\"Init\"";

    for (auto element: elements) {
        stream << ", " << element;
    }

    stream << "]";
}


MemberExpr::MemberExpr(TokenKind op, Expr* object, const Identifier& identifier, const Location& location)
    : Expr(location), op(op), object(object), identifier(identifier) {
}

MemberExpr::MemberExpr(TokenKind op, const Type* type, const Identifier& identifier, const Location& location)
    : Expr(location), op(op), type(type), identifier(identifier) {
}

VisitExpressionOutput MemberExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

void MemberExpr::print(ostream& stream) const {
    stream << "[\"";

    if (op == '.') {
        stream << '.';
    } else {
        stream << "->";
    }

    stream << "\", ";
    
    if (object) {
        stream << object;
    } else {
        stream << type;
    }
    
    stream << ", \"" << identifier << "\"]";
}


MoveExpr::MoveExpr(Expr* expr, const Location& location)
    : Expr(location), expr(expr) {
}

VisitExpressionOutput MoveExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

void MoveExpr::print(ostream& stream) const {
    stream << "[\"move\", " << expr << "]";
}


SizeOfExpr::SizeOfExpr(const Type* type, const Location& location)
    : Expr(location), type(type) {
}

VisitExpressionOutput SizeOfExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

void SizeOfExpr::print(ostream& stream) const {
    stream << "[\"sizeof\", " << type << "]";
}


SubscriptExpr::SubscriptExpr(Expr* left, Expr* right, const Location& location)
    : Expr(location), left(left), right(right) {
}

VisitExpressionOutput SubscriptExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

void SubscriptExpr::print(ostream& stream) const {
    stream << "[\"subs\", " << left << ", " << right << "]";
}


UnaryExpr::UnaryExpr(Expr* expr, TokenKind op, const Location& location)
    : Expr(location), expr(expr), op(op) {
}

VisitExpressionOutput UnaryExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

void UnaryExpr::print(ostream& stream) const {
    stream << "[\"" << char(op) << "\", " << expr << "]";
}


UninitializedExpr::UninitializedExpr(const Location& location): Expr(location) {
}

VisitExpressionOutput UninitializedExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

void UninitializedExpr::print(ostream& stream) const {
    stream << "[\"uninit\"]";
}
