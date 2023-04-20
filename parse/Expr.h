#ifndef PARSE_EXPR_H
#define PARSE_EXPR_H

#include "ASTNode.h"
#include "lex/Token.h"
#include "Type.h"

struct Decl;

struct AddressExpr: Expr {
    Expr* expr{};
    
    AddressExpr(Expr* expr, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) override;
    virtual void print(ostream& stream) const override;
};

struct BinaryExpr: Expr {
    Expr* left{};
    Expr* right{};
    TokenKind op{};
    
    BinaryExpr(Expr* left, Expr* right, TokenKind op, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) override;
    virtual void print(ostream& stream) const override;
};

struct CallExpr: Expr {
    Expr* function{};
    vector<Expr*> parameters;

    CallExpr(Expr* function, vector<Expr*>&& parameters, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) override;
    virtual void print(ostream& stream) const override;
};

struct CastExpr: Expr {
    const Type* type;
    Expr* expr{};
    
    CastExpr(const Type* type, Expr* expr, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) override;
    virtual void print(ostream& stream) const override;
};

struct ConditionExpr: Expr {
    Expr* condition{};
    Expr* then_expr{};
    Expr* else_expr{};

    ConditionExpr(Expr* condition, Expr* then_expr, Expr* else_expr, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) override;
    virtual void print(ostream& stream) const override;
};

struct DereferenceExpr: Expr {
    Expr* expr{};
    
    DereferenceExpr(Expr* expr, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) override;
    virtual void print(ostream& stream) const override;
};

struct EntityExpr: Expr {
    Declarator* declarator{};

    EntityExpr(Declarator* declarator, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) override;
    virtual void print(ostream& stream) const override;
};

struct InitializerExpr: Expr {
    vector<Expr*> elements;

    explicit InitializerExpr(const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) override;
    virtual void print(ostream& stream) const override;
};

struct SizeOfExpr: Expr {
    const Type* type{};

    SizeOfExpr(const Type* type, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) override;
    virtual void print(ostream& stream) const override;
};

struct SubscriptExpr: Expr {
    Expr* left{};
    Expr* right{};

    SubscriptExpr(Expr* left, Expr* right, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) override;
    virtual void print(ostream& stream) const override;
};

#endif
