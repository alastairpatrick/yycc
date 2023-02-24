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
    unsigned long long value;

    static IntegerConstant* of(const char* text, int token, const Location& location);

    IntegerConstant(unsigned long long int_value, const IntegerType* type, const Location& location);

    virtual const Type* get_type() const;
    virtual LLVMValueRef generate_value(CodeGenContext* context) const;
    virtual void print(ostream& stream) const;
};

struct FloatingPointConstant: Constant {
    const FloatingPointType* type{};
    double value;

    static FloatingPointConstant* of(const char* text, const Location& location);

    FloatingPointConstant(double float_value, const FloatingPointType* type, const Location& location);

    virtual const Type* get_type() const;
    virtual LLVMValueRef generate_value(CodeGenContext* context) const;
    virtual void print(ostream& stream) const;
};

struct StringConstant: Constant {
    const IntegerType* char_type{};
    string utf8_literal;

    static StringConstant* of(const char* text, size_t capacity_hint, const Location& location);

    StringConstant(string&& utf8_literal, const IntegerType* char_type, const Location& location);

    virtual const Type* get_type() const;
    virtual LLVMValueRef generate_value(CodeGenContext* context) const;
    virtual void print(ostream& stream) const;
};



#endif
