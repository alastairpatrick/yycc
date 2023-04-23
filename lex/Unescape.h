#ifndef LEX_UNESCAPE_H
#define LEX_UNESCAPE_H

struct Location;

struct CharValue {
    uint32_t code{};
    bool multi_byte{};
};

CharValue decode_char(string_view& source);

CharValue unescape_char(string_view& source, bool decode_multi_byte, const Location& location);
string unescape_string(string_view source, bool decode_multi_byte, const Location& location);

#endif
