#include "Printable.h"
#include "TranslationUnitContext.h"

Printable::~Printable() {
}

ostream& operator<<(ostream& stream, const Printable* p) {
    auto& printing = TranslationUnitContext::it->printing;
    auto it = printing.find(p);
    if (it != printing.end()) {
        return stream << "\"recursive\"";
    }

    printing.insert(p);

    if (p) {
        p->print(stream);
    } else {
        stream << "null";
    }

    printing.erase(p);

    return stream;
}
