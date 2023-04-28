#include "TextStream.h"

TextStream::TextStream(ostream& stream): stream(stream) {
    current_location.line = 1;
    current_location.column = 1;
}

static void output_line_column(ostream& stream, const Location& location) {
    stream << "#line " << location.line;
    if (location.column != 1) {
        stream << ' ' << location.column;
    }
}

static void output_filename(ostream& stream, string_view filename) {
    stream << " \"";
    for (char c : filename) {
        if (c == '\\') {
            stream << c;
        }
        stream << c;
    }
    stream << '\"';
}

void TextStream::locate(const Location& location) {
    if (location.filename != current_location.filename) {
        if (need_newline) stream << '\n';
        output_line_column(stream, location);
        output_filename(stream, *location.filename);
        stream << '\n';
        current_location = location;
        need_newline = false;
    } else if (location.line >= current_location.line && location.line - current_location.line < 5) {
        while (location.line > current_location.line) {
            stream << '\n';
            ++current_location.line;
            current_location.column = 1;
            need_newline = false;
        }
    } else if (location.line != current_location.line ||
        location.column > current_location.column ||
        current_location.column - location.column > 10) {
        if (need_newline) stream << '\n';
        output_line_column(stream, location);
        stream << '\n';
        current_location = location;
        need_newline = false;
    }

    while (current_location.column < location.column) {
        stream << ' ';
        ++current_location.column;
    }
}

void TextStream::write(string_view text) {
    stream << text;

    for (char c : text) {
        if (c == '\n') {
            ++current_location.line;
            current_location.column = 1;
        } else {
            ++current_location.column;
        }
    }

    if (text.length()) {
        need_newline = text.back() != '\n';
    }
}

void TextStream::new_line() {
    if (!need_newline) return;

    stream << '\n';
    ++current_location.line;
    current_location.column = 1;
}
