#include "Location.h"

bool operator<(const Location& a, const Location& b) {
    if (a.filename == b.filename) {
        if (a.line == b.line) {
            return a.column < b.column;
        }
        return a.line < b.line;
    }
    return *a.filename < *b.filename;
}

bool operator==(const Location& a, const Location& b) {
    return (a.line == b.line) && (a.column == b.column) && (a.filename == b.filename);
}
