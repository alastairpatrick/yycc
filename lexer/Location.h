#ifndef LEXER_LOCATION_H
#define LEXER_LOCATION_H

struct Location {
    size_t line{};
    size_t column{};
    string_view filename;
};

#endif
