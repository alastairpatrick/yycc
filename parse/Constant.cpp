#include "nlohmann/json.hpp"

#include "Constant.h"
#include "Expr.h"
#include "lex/StringLiteral.h"
#include "Message.h"
#include "TranslationUnitContext.h"
#include "visit/Visitor.h"

using json = nlohmann::json;

Constant::Constant(const Location& location): Expr(location) {
}

// C99 6.4.4.1p5
static const IntegerType* smallest_integer_type(IntegerSignedness min_signedness, IntegerSignedness max_signedness, IntegerSize target_size, unsigned long long value, const Location& location) {
    for (auto check_size = unsigned(target_size); check_size < unsigned(IntegerSize::NUM); ++check_size) {
        for (auto check_signedness = unsigned(min_signedness); check_signedness <= unsigned(max_signedness); ++check_signedness) {
            auto type = IntegerType::of(IntegerSignedness(check_signedness), IntegerSize(check_size));
            if (value <= type->max()) {
                return type;
            }
        }
    }

    return IntegerType::of(max_signedness, IntegerSize::LONG_LONG);
}

static IntegerConstant* parse_integer_literal(string_view text, int radix, const Location& location) {
    auto llvm_context = TranslationUnitContext::it->llvm_context;

    bool sign_suffix{};
    auto signedness = IntegerSignedness::SIGNED;
    int longs = 0;
    for (; text.size(); text.remove_suffix(1)) {
        char c = toupper(text.back());
        if (c == 'U') {
            signedness = IntegerSignedness::UNSIGNED;
            sign_suffix = true;
        } else if (c == 'L') {
            ++longs;
        } else {
            break;
        }
    }

    assert(longs <= 2);  // TODO: error
    auto target_size = IntegerSize::INT;
    if (longs >= 2) {
        target_size = IntegerSize::LONG_LONG;
    } else if (longs == 1) {
        target_size = IntegerSize::LONG;
    }

    auto largest_type = IntegerType::of(signedness, IntegerSize::LONG_LONG);
    auto value = LLVMConstIntOfStringAndSize(largest_type->llvm_type(), text.data(), text.size(), radix);
    auto value_int = LLVMConstIntGetZExtValue(value);

    // TODO error if value is too large for any integer type
    
    // C99 6.4.4.1p5
    const IntegerType* type{};
    if (radix == 10 || sign_suffix) {
        type = smallest_integer_type(signedness, signedness, target_size, value_int, location);
    } else {
        type = smallest_integer_type(IntegerSignedness::SIGNED, IntegerSignedness::UNSIGNED, target_size, value_int, location);
    }

    return new IntegerConstant(LLVMConstIntCast(value, type->llvm_type(), type->signedness == IntegerSignedness::SIGNED), type, location);
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
        c = unescape_char(text, true, location).code;
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

VisitStatementOutput IntegerConstant::accept(Visitor& visitor, const VisitStatementInput& input) {
    return visitor.visit(this, input);
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

VisitStatementOutput FloatingPointConstant::accept(Visitor& visitor, const VisitStatementInput& input) {
    return visitor.visit(this, input);
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

    auto value = unescape_string(text, is_wide, location);

    return new StringConstant(move(value), IntegerType::of_char(is_wide), location);
}

StringConstant::StringConstant(StringLiteral&& value, const IntegerType* character_type, const Location& location)
    : Constant(location), character_type(character_type), value(move(value)) {
}

VisitStatementOutput StringConstant::accept(Visitor& visitor, const VisitStatementInput& input) {
    return visitor.visit(this, input);
}

void StringConstant::print(ostream& stream) const {
    string t(value.chars);
    stream << "[\"S\", " << character_type << ", " << json(t) << ']';
}
