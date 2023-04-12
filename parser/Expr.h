#ifndef PARSER_EXPR_H
#define PARSER_EXPR_H

#include "ASTNode.h"
#include "lexer/Token.h"
#include "Type.h"

struct Decl;

struct Value {
    Value() = default;
    Value(LLVMValueRef value, const Type* type);

    bool is_const() const;
    bool is_const_integer() const;

    LLVMValueRef value{};
    const Type* type{};
};

struct ConditionExpr: Expr {
    ConditionExpr(Expr* condition, Expr* then_expr, Expr* else_expr, const Location& location);

    Expr* condition{};
    Expr* then_expr{};
    Expr* else_expr{};

    virtual Value emit(EmitContext& context) const;
    virtual void print(ostream& stream) const;
};

struct NameExpr: Expr {
    const Declarator* declarator{};

    NameExpr(const Declarator* declarator, const Location& location);

    virtual void print(ostream& stream) const;
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
    BinaryExpr(Expr* left, Expr* right, BinaryOp op, const Location& location);

    Expr* left{};
    Expr* right{};
    BinaryOp op;
    
    virtual Value emit(EmitContext& context) const;
    virtual void print(ostream& stream) const;
};

// The default value of a variable, e.g. zero for static duration and uninitialized for automatic duration
struct DefaultExpr: Expr {
    DefaultExpr(const Type* type, const Location& location);

    const Type* type;

    virtual Value emit(EmitContext& context) const;
    virtual void print(ostream& stream) const;
};

#endif
