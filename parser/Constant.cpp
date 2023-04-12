#include "nlohmann/json.hpp"

#include "Constant.h"
#include "Expr.h"
#include "lexer/Unescape.h"
#include "Message.h"

using json = nlohmann::json;

Constant::Constant(const Location& location): Expr(location) {
}

static IntegerConstant* parse_integer_literal(string_view text, int radix, const Location& location) {
    auto signedness = IntegerSignedness::SIGNED;
    int longs = 0;
    for (; text.size(); text.remove_suffix(1)) {
        char c = toupper(text.back());
        if (c == 'U') signedness = IntegerSignedness::UNSIGNED;
        else if (c == 'L') ++longs;
        else break;
    }

    assert(longs <= 2);  // TODO: error
    auto size = IntegerSize::INT;
    if (longs >= 2) size = IntegerSize::LONG_LONG;
    else if (longs == 1) size = IntegerSize::LONG;

    auto type = IntegerType::of(signedness, size);
    auto value = LLVMConstIntOfStringAndSize(type->llvm_type(), text.data(), text.size(), radix);
    return new IntegerConstant(value, type, location);
}

static IntegerConstant* parse_char_literal(string_view text, const Location& location) {
    bool is_wide = false;
    if (text[0] == 'L') {
        is_wide = true;
        text.remove_prefix(1);
    }

    assert(text[0] == '\'');
    text.remove_prefix(1);

    uint32_t c = 0;
    if (text[0] == '\'') {
        message(Severity::ERROR, location) << "character literal may only have one character\n";
    } else {
        c = unescape_char(text, location);
        if (text[0] != '\'') {
            message(Severity::ERROR, location) << "character literal may only have one character\n";
        }
    }

    auto type = IntegerType::of_char(is_wide);
    auto value = LLVMConstInt(type->llvm_type(), c, type->is_signed());
    return new IntegerConstant(value, type, location);
}

IntegerConstant* IntegerConstant::default_expr(const Location& location) {
    auto type = IntegerType::default_type();
    auto value = LLVMConstInt(type->llvm_type(), 0, type->is_signed());
    return new IntegerConstant(value, type, location);
}

IntegerConstant* IntegerConstant::of(string_view text, TokenKind token, const Location& location) {
    switch (token) {
      default:
        assert(false);
        return nullptr;
      case TOK_BIN_INT_LITERAL:
        text.remove_prefix(2);
        return parse_integer_literal(text, 2, location);
      case TOK_OCT_INT_LITERAL:
        return parse_integer_literal(text, 8, location);
      case TOK_DEC_INT_LITERAL:
        return parse_integer_literal(text, 10, location);
      case TOK_HEX_INT_LITERAL:
        text.remove_prefix(2);
        return parse_integer_literal(text, 16, location);
      case TOK_CHAR_LITERAL:
        return parse_char_literal(text, location);
    }
}

IntegerConstant::IntegerConstant(LLVMValueRef value, const IntegerType* type, const Location& location)
    : Constant(location), type(type), value(value) {
    assert(value);
}

Value IntegerConstant::evaluate_constant() const {
    return Value(value, type);
}

Value IntegerConstant::emit(EmitContext& context) const {
    return Value(value, type);
}

void IntegerConstant::print(ostream& stream) const {
    auto int_value = LLVMConstIntGetZExtValue(value);
    if (type == IntegerType::of(IntegerSignedness::SIGNED, IntegerSize::INT)) {
        stream << int_value;
    } else {
        stream << '[' << type << ", " << int_value << ']';
    }
}


FloatingPointConstant* FloatingPointConstant::of(string_view text, TokenKind token, const Location& location) {
    auto size = FloatingPointSize::DOUBLE;
    for (; text.size(); text.remove_suffix(1)) {
        char c = toupper(text.back());
        if (c == 'F') size = FloatingPointSize::FLOAT;
        else if (c == 'L') size = FloatingPointSize::LONG_DOUBLE;
        else break;
    }

    auto type = FloatingPointType::of(size);
    auto value = LLVMConstRealOfStringAndSize(type->llvm_type(), text.data(), text.size());
    return new FloatingPointConstant(value, type, location);
}


FloatingPointConstant::FloatingPointConstant(LLVMValueRef value, const FloatingPointType* type, const Location& location)
    : Constant(location), type(type), value(value) {
    assert(value);
}

Value FloatingPointConstant::emit(EmitContext& context) const {
    return Value(value, type);
}

void FloatingPointConstant::print(ostream& stream) const {
    LLVMBool loses_info;
    double float_value = LLVMConstRealGetDouble(value, &loses_info);
    stream << '[' << type << ", " << float_value << ']';
}

StringConstant* StringConstant::of(string_view text, const Location& location) {
    bool is_wide = false;
    if (text[0] == 'L') {
        is_wide = true;
        text.remove_prefix(1);
    }

    auto value = unescape_string(text, location);

    return new StringConstant(move(value), IntegerType::of_char(is_wide), location);
}

StringConstant::StringConstant(string&& utf8_literal, const IntegerType* char_type, const Location& location)
    : Constant(location), char_type(char_type), utf8_literal(move(utf8_literal)) {
}

void StringConstant::print(ostream& stream) const {
    string t(utf8_literal);
    stream << "[\"S\", " << char_type << ", " << json(t) << ']';
}

Value StringConstant::emit(EmitContext& context) const {
    assert(false);
    return Value();
}
