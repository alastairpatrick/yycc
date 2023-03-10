#ifndef CONSTANT_H
#define CONSTANT_H

#include "ASTNode.h"
#include "Token.h"
#include "Type.h"

struct Constant: Expr {
    Location location;

    Constant(const Location& location);
};

struct IntegerConstant: Constant {
    const IntegerType* type{};
    LLVMValueRef value;

    static IntegerConstant* default_expr(const Location& location);
    static IntegerConstant* of(string_view text, TokenKind token, const Location& location);

    IntegerConstant(LLVMValueRef value, const IntegerType* type, const Location& location);

    virtual const Type* get_type() const;
    virtual LLVMValueRef generate_value(CodeGenContext* context) const;
    virtual void print(ostream& stream) const;
};

struct FloatingPointConstant: Constant {
    const FloatingPointType* type{};
    LLVMValueRef value;

    static FloatingPointConstant* of(string_view text, TokenKind token, const Location& location);

    FloatingPointConstant(LLVMValueRef value, const FloatingPointType* type, const Location& location);

    virtual const Type* get_type() const;
    virtual LLVMValueRef generate_value(CodeGenContext* context) const;
    virtual void print(ostream& stream) const;
};

struct StringConstant: Constant {
    const IntegerType* char_type{};
    string utf8_literal;

    static StringConstant* of(string_view text, const Location& location);

    StringConstant(string&& utf8_literal, const IntegerType* char_type, const Location& location);

    virtual const Type* get_type() const;
    virtual LLVMValueRef generate_value(CodeGenContext* context) const;
    virtual void print(ostream& stream) const;
};



#endif
