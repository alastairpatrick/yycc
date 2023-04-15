#ifndef PARSER_CONSTANT_H
#define PARSER_CONSTANT_H

#include "ASTNode.h"
#include "lexer/Token.h"
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
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) override;

    virtual Value emit(EmitContext& context) const override;
    virtual void print(ostream& stream) const override;
};

struct FloatingPointConstant: Constant {
    const FloatingPointType* type{};
    LLVMValueRef value;

    static FloatingPointConstant* of(string_view text, TokenKind token, const Location& location);

    FloatingPointConstant(LLVMValueRef value, const FloatingPointType* type, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) override;

    virtual Value emit(EmitContext& context) const override;
    virtual void print(ostream& stream) const override;
};

struct StringConstant: Constant {
    const IntegerType* char_type{};
    string utf8_literal;

    static StringConstant* of(string_view text, const Location& location);

    StringConstant(string&& utf8_literal, const IntegerType* char_type, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) override;

    virtual Value emit(EmitContext& context) const override;
    virtual void print(ostream& stream) const override;
};



#endif
