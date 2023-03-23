#ifndef TEXT_STREAM_H
#define TEXT_STREAM_H

#include "lexer/Location.h"

// Writes program text to a stream, maintaining location by synthesizing white space tokens and #line directives.
struct TextStream {
    explicit TextStream(ostream& stream);
    void operator=(const TextStream) = delete;

    ostream& stream;
    Location current_location;

    void write(string_view text, const Location& location);
};

#endif
