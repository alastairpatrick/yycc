#ifndef PARSE_CONSTANT_H
#define PARSE_CONSTANT_H

#include "ASTNode.h"
#include "lex/Token.h"
#include "lex/StringLiteral.h"
#include "Type.h"
#include "Value.h"

struct Constant: Expr {
    Constant(const Location& location);
};

struct IntegerConstant: Constant {
    Value value;

    static IntegerConstant* default_expr(const Location& location);
    static IntegerConstant* of(string_view text, TokenKind token, const Location& location);
    static IntegerConstant* of(const IntegerType* type, unsigned long long value, const Location& location);

    IntegerConstant(Value value, const Location& location);
    virtual VisitExpressionOutput accept(Visitor& visitor) override;
    virtual void print(ostream& stream) const override;
};

struct FloatingPointConstant: Constant {
    Value value;

    static FloatingPointConstant* of(string_view text, TokenKind token, const Location& location);

    FloatingPointConstant(Value value, const Location& location);
    virtual VisitExpressionOutput accept(Visitor& visitor) override;

    virtual void print(ostream& stream) const override;
};

struct StringConstant: Constant {
    // If character type is 'char', it's impossible to know what the encoding is.
    // It doesn't actually matter what the encoding is because the value is an image of the data to
    // add to the module.
    // 
    // If the character type is other than 'char' then the encoding is UTF-8.
    //
    // The character type is never 'signed char' or 'unsigned char' because C has no way to express
    // the signedness of characters in a string literal.
    const IntegerType* const character_type{};
    const StringLiteral value;

    static StringConstant* of(string_view text, const Location& location);

    StringConstant(StringLiteral&& value, const IntegerType* character_type, const Location& location);
    virtual VisitExpressionOutput accept(Visitor& visitor) override;

    virtual void print(ostream& stream) const override;
};

#endif
