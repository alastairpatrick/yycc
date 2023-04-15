#ifndef PARSER_EXPR_H
#define PARSER_EXPR_H

#include "ASTNode.h"
#include "lexer/Token.h"
#include "Type.h"

struct Decl;

struct Value {
    bool is_const() const;
    bool is_const_integer() const;

    Value() = default;
    Value(const Type* type, LLVMValueRef llvm = nullptr);

    LLVMValueRef llvm{};
    const Type* type{};
};

struct ConditionExpr: Expr {
    Expr* condition{};
    Expr* then_expr{};
    Expr* else_expr{};

    ConditionExpr(Expr* condition, Expr* then_expr, Expr* else_expr, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) override;
    virtual void print(ostream& stream) const override;
};

struct EntityExpr: Expr {
    Declarator* declarator{};

    EntityExpr(Declarator* declarator, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) override;
    virtual void print(ostream& stream) const override;
};

enum class BinaryOp {
    LOGICAL_OR			= TOK_OR_OP,
    LOGICAL_AND			= TOK_AND_OP,
    ADD					= '+',
    SUB					= '-',
    MUL					= '*',
    DIV					= '/',
    MOD					= '%',
};

struct BinaryExpr: Expr {
    Expr* left{};
    Expr* right{};
    BinaryOp op;
    
    BinaryExpr(Expr* left, Expr* right, BinaryOp op, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) override;
    virtual void print(ostream& stream) const override;
};

struct SizeOfExpr: Expr {
    const Type* type;

    SizeOfExpr(const Type* type, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) override;
    virtual void print(ostream& stream) const override;
};

struct InitializerExpr: Expr {
    vector<Expr*> elements;

    explicit InitializerExpr(const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) override;
    virtual void print(ostream& stream) const override;
};

#endif
