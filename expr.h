#ifndef EXPR_H
#define EXPR_H

#include "ASTNode.h"
#include "Token.h"
#include "Type.h"

struct Decl;

struct ConditionExpr: Expr {
    ConditionExpr(Expr* condition, Expr* then_expr, Expr* else_expr, const Location& location);

    Expr* condition{};
    Expr* then_expr{};
    Expr* else_expr{};

    virtual const Type* get_type() const;
    virtual LLVMValueRef generate_value(CodeGenContext* context) const;
    virtual void print(std::ostream& stream) const;
};

struct Constant: Expr {
    Location location;

    Constant(const Location& location);
};

struct IntegerConstant: Constant {
    const IntegerType* type{};
    unsigned long long value;

    IntegerConstant(unsigned long long int_value, const IntegerType* type, const Location& location);

    virtual const Type* get_type() const;
    virtual LLVMValueRef generate_value(CodeGenContext* context) const;
    virtual void print(std::ostream& stream) const;
};

struct FloatingPointConstant: Constant {
    const FloatingPointType* type{};
    double value;

    FloatingPointConstant(double float_value, const FloatingPointType* type, const Location& location);

    virtual const Type* get_type() const;
    virtual LLVMValueRef generate_value(CodeGenContext* context) const;
    virtual void print(std::ostream& stream) const;
};

struct StringConstant: Constant {
    const IntegerType* char_type{};
    std::string utf8_literal;

    StringConstant(std::string utf8_literal, const IntegerType* char_type, const Location& location);

    virtual const Type* get_type() const;
    virtual LLVMValueRef generate_value(CodeGenContext* context) const;
    virtual void print(std::ostream& stream) const;
};

struct NameExpr: Expr {
    const Decl* decl{};

    NameExpr(const Decl* decl, const Location& location);

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
    BinaryExpr(Expr* left, Expr* right, BinaryOp op, const Location& location);

    Expr* left{};
    Expr* right{};
    BinaryOp op;
    
    virtual const Type* get_type() const;
    virtual LLVMValueRef generate_value(CodeGenContext* context) const;
    virtual void print(std::ostream& stream) const;
};

// The default value of a variable, e.g. zero for static duration and uninitialized for automatic duration
struct DefaultExpr: Expr {
    DefaultExpr(const Type* type, const Location& location);

    const Type* type;

    virtual const Type* get_type() const;
    virtual LLVMValueRef generate_value(CodeGenContext* context) const;
    virtual void print(std::ostream& stream) const;
};

#endif
