#include "std.h"

#include "Printable.h"

Printable::~Printable() {
}

ostream& operator<<(ostream& stream, const Printable* p) {
    if (p) {
        p->print(stream);
    } else {
        stream << "null";
    }
    return stream;
}

ostream& operator<<(ostream& stream, const string* p) {
    if (p) {
        stream << *p;
    } else {
        stream << "null";
    }
    return stream;
}
