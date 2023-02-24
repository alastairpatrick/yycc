#include "nlohmann/json.hpp"

#include "Constant.h"
#include "CompileContext.h"

using json = nlohmann::json;

Constant::Constant(const Location& location): Expr(location) {
}

static IntegerConstant* parse_integer_literal(const char* text, int radix, const Location& location) {
    char* p;
    auto value = strtoull(text, &p, radix);
    if (errno == ERANGE) {
        // TODO: warn about truncation of int literal
    }

    auto signedness = IntegerSignedness::SIGNED;
    int longs = 0;
    for (; *p; ++p) {
        char c = toupper(*p);
        if (c == 'U') signedness = IntegerSignedness::UNSIGNED;
        else if (c == 'L') ++longs;
        else assert(false);
    }

    assert(longs <= 2);
    auto size = IntegerSize::INT;
    if (longs >= 2) size = IntegerSize::LONG_LONG;
    else if (longs == 1) size = IntegerSize::LONG;

    // TODO: should be checking against target max value.
    unsigned long long max_value;
    if (size == IntegerSize::INT) {
        max_value = UINT_MAX;
    } else if (size == IntegerSize::LONG) {
        max_value = ULONG_MAX;
    } else {
        max_value = ULLONG_MAX;
    }
    
    if (value > max_value) {
        // TODO: warn about truncation of int literal
        value = max_value;
    }

    return new IntegerConstant(value, IntegerType::of(signedness, size), location);
}

static wchar_t parse_char_code(const char** text, int num_digits, int radix, const Location& location) {
    char digits[9];
    assert(num_digits <= 8);

    const char* src = *text;
    int i;
    for (i = 0; i < num_digits; ++i) {
        if (src[i] == 0) break;
        digits[i] = src[i];
    }
    digits[i] = 0;

    char* end = nullptr;
    wchar_t result = wchar_t(strtol(digits, &end, radix));

    if (end - digits != num_digits) {
        message(Severity::ERROR, location) << "truncated character escape code\n";
    }
    *text += end - digits;

    return result;
}

static uint32_t unescape_char(const char** p, const Location& location) {
    uint32_t c = **p;
    (*p)++;

    if (c == '\\') {
        c = **p;
        (*p)++;

        switch (c) {
        case 'a':
            c = '\a';
            break;
        case 'b':
            c = '\b';
            break;
        case 'f':
            c = '\f';
            break;
        case 'n':
            c = '\n';
            break;
        case 'r':
            c = '\r';
            break;
        case 't':
            c = '\t';
            break;
        case 'v':
            c = '\v';
            break;
        case 'u':
            c = parse_char_code(p, 4, 16, location);
            break;
        case 'U':
            c = parse_char_code(p, 8, 16, location);
            break;
        case 'x':
            c = parse_char_code(p, 2, 16, location);
            break;
        case '\'':
        case '"':
        case '?':
        case '\\':
            break;
        default:
            if (c >= '0' && c <= '9') {
                (*p)--;
                c = parse_char_code(p, 3, 8, location);
                break;
            }
            message(Severity::ERROR, location) << "unrecognized escape sequence\n";
        }

        return c;
    } else if ((c & 0x80) == 0) {
        return c;
    } else {
        char b1 = **p;
        (*p)++;
        if ((b1 & 0xC0) != 0x80) return 0;
        if ((c & 0xE0) == 0xC0) return ((c & 0x1F) << 6) | (b1 & 0x3F);

        char b2 = **p;
        (*p)++;
        if ((b2 & 0xC0) != 0x80) return 0;
        if ((c & 0xF0) == 0xE0) return ((c & 0xF) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);

        char b3 = **p;
        (*p)++;
        if ((b3 & 0xC0) != 0x80) return 0;
        return ((c & 0b111) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);;
    }

    return 0;
}

static IntegerConstant* parse_char_literal(const char* text, const Location& location) {
    const char* p = text;
    bool is_wide = false;
    if (*p == 'L') {
        is_wide = true;
        ++p;
    }

    assert(*p == '\'');
    ++p;

    uint32_t c = 0;
    if (*p == '\'') {
        message(Severity::ERROR, location) << "character literal may only have one character\n";
    } else {
        c = unescape_char(&p, location);
        if (*p != '\'') {
            message(Severity::ERROR, location) << "character literal may only have one character\n";
        }
    }

    return new IntegerConstant(c, IntegerType::of_char(is_wide), location);
}

IntegerConstant* IntegerConstant::of(const char* text, int token, const Location& location) {
    switch (token) {
    case TOK_BIN_INT_LITERAL:
        return parse_integer_literal(text+2, 2, location);
    case TOK_OCT_INT_LITERAL:
        return parse_integer_literal(text, 8, location);
    case TOK_DEC_INT_LITERAL:
        return parse_integer_literal(text, 10, location);
    case TOK_HEX_INT_LITERAL:
        return parse_integer_literal(text+2, 16, location);
    case TOK_CHAR_LITERAL:
        return parse_char_literal(text, location);
    default:
        assert(false);
        return nullptr;
    }
}

IntegerConstant::IntegerConstant(unsigned long long value, const IntegerType* type, const Location& location)
    : Constant(location), type(type), value(value) {
}

const Type* IntegerConstant::get_type() const {
    return type;
}

LLVMValueRef IntegerConstant::generate_value(CodeGenContext* context) const {
    return LLVMConstInt(type->llvm_type(), value, type->is_signed());
}

void IntegerConstant::print(ostream& stream) const {
    stream << '"' << type << value << '"';
}


FloatingPointConstant* FloatingPointConstant::of(const char* text, const Location& location) {
    char* p;
    double value = strtod(text, &p);
    if (errno == ERANGE) {
    }

    auto size = FloatingPointSize::DOUBLE;
    for (; *p; ++p) {
        char c = toupper(*p);
        if (c == 'F') size = FloatingPointSize::FLOAT;
        else if (c == 'L') size = FloatingPointSize::LONG_DOUBLE;
        else assert(false);
    }

    return new FloatingPointConstant(value, FloatingPointType::of(size), location);
}


FloatingPointConstant::FloatingPointConstant(double value, const FloatingPointType* type, const Location& location)
    : Constant(location), type(type), value(value) {
}

const Type* FloatingPointConstant::get_type() const {
    return type;
}

LLVMValueRef FloatingPointConstant::generate_value(CodeGenContext* context) const {
    return LLVMConstReal(type->llvm_type(), value);
}

void FloatingPointConstant::print(ostream& stream) const {
    stream << '"' << type << value << '"';
}

string parse_string(const char* text, size_t capacity_hint, const Location& location) {
    std::string value;
    value.reserve(capacity_hint);

    const char* p = text;
    assert(*p == '"');
    ++p;

    while (*p != '"') {
        assert(*p);

        auto c = unescape_char(&p, location);
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

StringConstant* StringConstant::of(const char* text, size_t capacity_hint, const Location& location) {
    const char* p = text;
    bool is_wide = false;
    if (*p == 'L') {
        is_wide = true;
        ++p;
    }

    auto value = parse_string(p, capacity_hint, location);

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
