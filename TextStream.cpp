#include "TextStream.h"

TextStream::TextStream(ostream& stream): stream(stream) {
    current_location.line = 1;
    current_location.column = 1;
}

void TextStream::locate(const Location& location) {
    if (location.filename != current_location.filename) {
        stream << "\n#line " << location.line << " \"" << *location.filename << "\"\n";
        current_location = location;
        current_location.column = 1;
    } else if (location.line - current_location.line >= 0 && location.line - current_location.line < 5) {
        while (location.line > current_location.line) {
            stream << '\n';
            ++current_location.line;
            current_location.column = 1;
        }
    } else if (location.line != current_location.line) {
        stream << "\n#line " << location.line << "\n";
        current_location = location;
        current_location.column = 1;
    }

    while (current_location.column < location.column) {
        stream << ' ';
        ++current_location.column;
    }
}

void TextStream::write(string_view text) {
    stream << text;
    current_location.column += text.length();
}
