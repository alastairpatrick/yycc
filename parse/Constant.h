#ifndef PARSE_CONSTANT_H
#define PARSE_CONSTANT_H

#include "ASTNode.h"
#include "lex/Token.h"
#include "lex/Unescape.h"
#include "Type.h"

struct Constant: Expr {
    Constant(const Location& location);
};

struct IntegerConstant: Constant {
    const IntegerType* type{};
    LLVMValueRef value;

    static IntegerConstant* default_expr(const Location& location);
    static IntegerConstant* of(string_view text, TokenKind token, const Location& location);

    IntegerConstant(LLVMValueRef value, const IntegerType* type, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) override;

    virtual void print(ostream& stream) const override;
};

struct FloatingPointConstant: Constant {
    const FloatingPointType* type{};
    LLVMValueRef value;

    static FloatingPointConstant* of(string_view text, TokenKind token, const Location& location);

    FloatingPointConstant(LLVMValueRef value, const FloatingPointType* type, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) override;

    virtual void print(ostream& stream) const override;
};

struct StringConstant: Constant {
    // If character type is "char", the character encoding is unknown. It might or might not be multi-byte.
    // The encoding doesn't matter because this is an image of the string constant to add to the module.
    // If the character type is other than "char" then the character encoding is UTF-8.
    const IntegerType* const character_type{};
    const StringLiteral value;

    static StringConstant* of(string_view text, const Location& location);

    StringConstant(StringLiteral&& value, const IntegerType* character_type, const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) override;

    virtual void print(ostream& stream) const override;
};

#endif
