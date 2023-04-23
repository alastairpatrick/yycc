#include "StringLiteral.h"

#include "Location.h"
#include "Message.h"

static uint32_t parse_char_code(string_view& text, size_t num_digits, int radix, const Location& location) {
    uint32_t c = 0;
    auto result = from_chars(text.data(), text.data() + min(text.size(), num_digits), c, radix);

    if (result.ec != errc{} || result.ptr != text.data() + num_digits) {
        message(Severity::ERROR, location) << "truncated character escape code\n";
    }

    text.remove_prefix(result.ptr - text.data());

    return c;
}

static uint32_t decode_utf8(uint32_t c, string_view& rest) {
    assert((c & 0x80) != 0);

    char b1 = rest[0];
    rest.remove_prefix(1);
    if ((b1 & 0xC0) != 0x80) return 0;
    if ((c & 0xE0) == 0xC0) return ((c & 0x1F) << 6) | (b1 & 0x3F);

    char b2 = rest[0];
    rest.remove_prefix(1);
    if ((b2 & 0xC0) != 0x80) return 0;
    if ((c & 0xF0) == 0xE0) return ((c & 0xF) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);

    char b3 = rest[0];
    rest.remove_prefix(1);
    if ((b3 & 0xC0) != 0x80) return 0;
    return ((c & 0b111) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);;
}

CharLiteral decode_char(string_view& source) {
    CharLiteral result{};
    uint32_t c = source[0];
    source.remove_prefix(1);
    if ((c & 0x80) != 0) {
        c = decode_utf8(c, source);
        result.multi_byte = true;
    }
    result.code = c;
    return result;
}

CharLiteral unescape_char(string_view& source, bool decode_multi_byte, const Location& location) {
    CharLiteral result;
    uint32_t c = source[0];
    source.remove_prefix(1);

    if (c == '\\') {
        c = source[0];

        switch (c) {
            default: {
              if (c >= '0' && c <= '9') {
                  c = parse_char_code(source, 3, 8, location);
                  break;
              }
              message(Severity::ERROR, location) << "unrecognized escape sequence\n";
              break;
          } case 'a': {
              source.remove_prefix(1);
              c = '\a';
              break;
          } case 'b': {
              source.remove_prefix(1);
              c = '\b';
              break;
          } case 'f': {
              source.remove_prefix(1);
              c = '\f';
              break;
          } case 'n': {
              source.remove_prefix(1);
              c = '\n';
              break;
          } case 'r': {
              source.remove_prefix(1);
              c = '\r';
              break;
          } case 't': {
              source.remove_prefix(1);
              c = '\t';
              break;
          } case 'v': {
              source.remove_prefix(1);
              c = '\v';
              break;
          } case 'u': {
              source.remove_prefix(1);
              c = parse_char_code(source, 4, 16, location);
              result.multi_byte = true;
              break;
          } case 'U': {
              source.remove_prefix(1);
              c = parse_char_code(source, 8, 16, location);
              result.multi_byte = true;
              break;
          } case 'x': {
              source.remove_prefix(1);
              c = parse_char_code(source, 2, 16, location);
              break;
          } case '\'':
            case '"':
            case '?':
            case '\\': {
              source.remove_prefix(1);
              break;
          }
        }
    } else if (decode_multi_byte && (c & 0x80) != 0) {
        c = decode_utf8(c, source);
        result.multi_byte = true;
    }

    result.code = c;
    return result;
}

StringLiteral unescape_string(string_view source, bool wide, const Location& location) {
    StringLiteral result;
    result.chars.reserve(source.size());

    assert(source[0] == '"');
    source.remove_prefix(1);

    while (source[0] != '"') {
        assert(source.size());
        auto value = unescape_char(source, wide, location);

        if (wide || value.multi_byte) {
            auto c = value.code;
            if (c < 0x80) {
                result.chars += c;
            } else if (c < 0x800) {
                result.chars += 0xC0 | (c >> 6);
                result.chars += 0x80 | (c & 0x3F);
            } else if (c < 0x10000) {
                result.chars += 0xE0 | (c >> 12);
                result.chars += 0x80 | ((c >> 6) & 0x3F);
                result.chars += 0x80 | (c & 0x3F);
            } else {
                result.chars += 0xF0 | (c >> 18);
                result.chars += 0x80 | ((c >> 12) & 0x3F);
                result.chars += 0x80 | ((c >> 6) & 0x3F);
                result.chars += 0x80 | (c & 0x3F);
            }
        } else {
            result.chars += value.code;
        }
        ++result.length;
    }

    return result;
}
