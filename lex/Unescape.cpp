#include "Unescape.h"

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

uint32_t unescape_char(string_view& text, const Location& location) {
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
