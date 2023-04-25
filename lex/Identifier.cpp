#include "Identifier.h"

ostream& operator<<(ostream& stream, const Identifier& identifier) {
    return stream << *identifier.name;
}

const char* identifier_name(const Identifier& identifier) {
    auto name = identifier.name;
    if (name->empty()) return "";

    return name->data();
}
