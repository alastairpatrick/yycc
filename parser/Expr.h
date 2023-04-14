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
    Value(LLVMValueRef value, const Type* type);

    LLVMValueRef value{};
    const Type* type{};
};

struct ConditionExpr: Expr {
    Expr* condition{};
    Expr* then_expr{};
    Expr* else_expr{};

    ConditionExpr(Expr* condition, Expr* then_expr, Expr* else_expr, const Location& location);

    virtual Value emit(EmitContext& context) const override;
    virtual void print(ostream& stream) const override;
};

struct EntityExpr: Expr {
    const Declarator* declarator{};

    EntityExpr(const Declarator* declarator, const Location& location);

    virtual Value emit(EmitContext& context) const override;
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

    virtual Value emit(EmitContext& context) const override;
    virtual void print(ostream& stream) const override;
};

struct SizeOfExpr: Expr {
    const Type* type;

    SizeOfExpr(const Type* type, const Location& location);
    virtual void resolve(ResolutionContext& context) override;
    virtual Value emit(EmitContext& context) const override;
    virtual void print(ostream& stream) const override;
};

struct InitializerExpr: Expr {
    vector<Expr*> elements;

    explicit InitializerExpr(const Location& location);
    virtual void print(ostream& stream) const override;
};

// The default value of a variable, e.g. zero for static duration and uninitialized for automatic duration
struct DefaultExpr: Expr {
    const Type* type;

    DefaultExpr(const Type* type, const Location& location);

    virtual Value emit(EmitContext& context) const override;
    virtual void print(ostream& stream) const override;
};

#endif
