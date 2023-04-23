#ifndef LEX_UNESCAPE_H
#define LEX_UNESCAPE_H

struct Location;

struct CharValue {
    uint32_t code{};
    bool multi_byte{};
};

struct StringLiteral {
    string chars{};
    size_t length{};
};

CharValue decode_char(string_view& source);

CharValue unescape_char(string_view& source, bool decode_multi_byte, const Location& location);
StringLiteral unescape_string(string_view source, bool decode_multi_byte, const Location& location);

#endif
