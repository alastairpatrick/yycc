#ifndef TEXT_STREAM_H
#define TEXT_STREAM_H

#include "lex/Location.h"

// Writes program text to a stream, maintaining location by synthesizing white space tokens and #line directives.
struct TextStream {
    explicit TextStream(ostream& stream);
    void operator=(const TextStream) = delete;

    ostream& stream;
    Location current_location;
    bool need_newline{};

    void locate(const Location& location);
    void write(string_view text);
};

#endif
