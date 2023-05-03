#include "Identifier.h"

ostream& operator<<(ostream& stream, const Identifier& identifier) {
    return stream << *identifier.text;
}

const char* Identifier::c_str() const {
    if (text->empty()) return "";
    return text->data();
}
