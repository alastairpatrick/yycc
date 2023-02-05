#include "ASTNode.h"

ostream& operator<<(ostream& stream, const ASTNodeVector& items) {
    stream << '[';
    for (auto i = 0; i < items.size(); ++i) {
        if (i != 0) stream << ", ";
        stream << items[i];
    }
    return stream << ']';
}
