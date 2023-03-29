#ifndef LEXER_UNESCAPE_H
#define LEXER_UNESCAPE_H

struct Location;

uint32_t unescape_char(string_view& text, const Location& location);
string unescape_string(string_view text, const Location& location);

#endif
