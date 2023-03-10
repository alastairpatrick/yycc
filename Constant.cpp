#include "nlohmann/json.hpp"

#include "Constant.h"
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

static uint32_t parse_char_code(string_view& text, size_t num_digits, int radix, const Location& location) {
    uint32_t c = 0;
    auto result = from_chars(text.data(), text.data() + min(text.size(), num_digits), c, radix);

    if (result.ec != errc{} || result.ptr != text.data() + num_digits) {
        message(Severity::ERROR, location) << "truncated character escape code\n";
    }

    text.remove_prefix(result.ptr - text.data());

    return c;
}

static uint32_t unescape_char(string_view& text, const Location& location) {
    uint32_t c = text[0];
    text.remove_prefix(1);

    if (c == '\\') {
        c = text[0];

        switch (c) {
            default: {
              if (c >= '0' && c <= '9') {
                  c = parse_char_code(text, 3, 8, location);
                  break;
              }
              message(Severity::ERROR, location) << "unrecognized escape sequence\n";
              break;
          } case 'a': {
              text.remove_prefix(1);
              c = '\a';
              break;
          } case 'b': {
              text.remove_prefix(1);
              c = '\b';
              break;
          } case 'f': {
              text.remove_prefix(1);
              c = '\f';
              break;
          } case 'n': {
              text.remove_prefix(1);
              c = '\n';
              break;
          } case 'r': {
              text.remove_prefix(1);
              c = '\r';
              break;
          } case 't': {
              text.remove_prefix(1);
              c = '\t';
              break;
          } case 'v': {
              text.remove_prefix(1);
              c = '\v';
              break;
          } case 'u': {
              text.remove_prefix(1);
              c = parse_char_code(text, 4, 16, location);
              break;
          } case 'U': {
              text.remove_prefix(1);
              c = parse_char_code(text, 8, 16, location);
              break;
          } case 'x': {
              text.remove_prefix(1);
              c = parse_char_code(text, 2, 16, location);
              break;
          } case '\'':
            case '"':
            case '?':
            case '\\': {
              text.remove_prefix(1);
              break;
          }
        }

        return c;
    } else if ((c & 0x80) == 0) {
        return c;
    } else {
        char b1 = text[0];
        text.remove_prefix(1);
        if ((b1 & 0xC0) != 0x80) return 0;
        if ((c & 0xE0) == 0xC0) return ((c & 0x1F) << 6) | (b1 & 0x3F);

        char b2 = text[0];
        text.remove_prefix(1);
        if ((b2 & 0xC0) != 0x80) return 0;
        if ((c & 0xF0) == 0xE0) return ((c & 0xF) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);

        char b3 = text[0];
        text.remove_prefix(1);
        if ((b3 & 0xC0) != 0x80) return 0;
        return ((c & 0b111) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);;
    }

    return 0;
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
    default:
        assert(false);
        return nullptr;
    }
}

IntegerConstant::IntegerConstant(LLVMValueRef value, const IntegerType* type, const Location& location)
    : Constant(location), type(type), value(value) {
    assert(value);
}

const Type* IntegerConstant::get_type() const {
    return type;
}

LLVMValueRef IntegerConstant::generate_value(CodeGenContext* context) const {
    return value;
}

void IntegerConstant::print(ostream& stream) const {
    auto int_value = LLVMConstIntGetZExtValue(value);
    stream << '"' << type << int_value << '"';
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

const Type* FloatingPointConstant::get_type() const {
    return type;
}

LLVMValueRef FloatingPointConstant::generate_value(CodeGenContext* context) const {
    return value;
}

void FloatingPointConstant::print(ostream& stream) const {
    LLVMBool loses_info;
    double float_value = LLVMConstRealGetDouble(value, &loses_info);
    stream << '"' << type << float_value << '"';
}

string unescape_string(string_view text, const Location& location) {
    std::string value;
    value.reserve(text.size());

    assert(text[0] == '"');
    text.remove_prefix(1);

    while (text[0] != '"') {
        assert(text.size());

        auto c = unescape_char(text, location);
        if (c < 0x80) {
            value += c;
        } else if (c < 0x800) {
            value += 0xC0 | (c >> 6);
            value += 0x80 | (c & 0x3F);
        } else if (c < 0x10000) {
            value += 0xE0 | (c >> 12);
            value += 0x80 | ((c >> 6) & 0x3F);
            value += 0x80 | (c & 0x3F);
        } else {
            value += 0xF0 | (c >> 18);
            value += 0x80 | ((c >> 12) & 0x3F);
            value += 0x80 | ((c >> 6) & 0x3F);
            value += 0x80 | (c & 0x3F);
        }
    }

    return value;
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

const Type* StringConstant::get_type() const {
    return char_type->pointer_to();
}

void StringConstant::print(ostream& stream) const {
    stringstream s;
    s << 'S' << char_type << utf8_literal;
    string t(s.str());
    stream << json(t);
}

LLVMValueRef StringConstant::generate_value(CodeGenContext* context) const {
    return nullptr;
}
