#ifndef EXPR_H
#define EXPR_H

#include "ASTNode.h"
#include "Token.h"

struct ConditionExpr: Expr {
    ConditionExpr(shared_ptr<Expr> condition, shared_ptr<Expr> then_expr, shared_ptr<Expr> else_expr, const Location& location);

    shared_ptr<Expr> condition;
    shared_ptr<Expr> then_expr;
    shared_ptr<Expr> else_expr;

    virtual const Type* get_type() const;
    virtual LLVMValueRef generate_value(CodeGenContext* context) const;
    virtual void print(std::ostream& stream) const;
};

struct Constant: Expr {
    Location location;

    Constant(const Location& location);
};

struct IntegerConstant: Constant {
    const IntegerType* type;
    unsigned long long value;

    IntegerConstant(unsigned long long int_value, const IntegerType* type, const Location& location);

    virtual const Type* get_type() const;
    virtual LLVMValueRef generate_value(CodeGenContext* context) const;
    virtual void print(std::ostream& stream) const;
};

struct FloatingPointConstant: Constant {
    const FloatingPointType* type;
    double value;

    FloatingPointConstant(double float_value, const FloatingPointType* type, const Location& location);

    virtual const Type* get_type() const;
    virtual LLVMValueRef generate_value(CodeGenContext* context) const;
    virtual void print(std::ostream& stream) const;
};

struct StringConstant: Constant {
    const IntegerType* char_type;
    std::string utf8_literal;

    StringConstant(std::string utf8_literal, const IntegerType* char_type, const Location& location);

    virtual const Type* get_type() const;
    virtual LLVMValueRef generate_value(CodeGenContext* context) const;
    virtual void print(std::ostream& stream) const;
};

struct NameExpr: Expr {
    const string* name;

    NameExpr(const string* name, const Location& location);

    virtual const Type* get_type() const;
    virtual LLVMValueRef generate_value(CodeGenContext* context) const;
    virtual void print(std::ostream& stream) const;
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
    BinaryExpr(shared_ptr<Expr> left, shared_ptr<Expr> right, BinaryOp op, const Location& location);

    shared_ptr<Expr> left;
    shared_ptr<Expr> right;
    BinaryOp op;
    Location location;
    
    virtual const Type* get_type() const;
    virtual LLVMValueRef generate_value(CodeGenContext* context) const;
    virtual void print(std::ostream& stream) const;
};


#endif
