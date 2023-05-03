#include "Identifier.h"

ostream& operator<<(ostream& stream, const Identifier& identifier) {
    return stream << *identifier.name;
}

const char* Identifier::c_str() const {
    if (name->empty()) return "";

    return name->data();
}
