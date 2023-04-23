#ifndef LEX_STRING_LITERAL_H
#define LEX_STRING_LITERAL_H

struct Location;

struct CharLiteral {
    uint32_t code{};
    bool multi_byte{};
};

struct StringLiteral {
    string chars{};
    size_t length{};
};

CharLiteral decode_char(string_view& source);

CharLiteral unescape_char(string_view& source, bool decode_multi_byte, const Location& location);
StringLiteral unescape_string(string_view source, bool decode_multi_byte, const Location& location);

#endif
