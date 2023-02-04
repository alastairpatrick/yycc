#include "AST.h"

ostream& operator<<(ostream& stream, StorageClass storage_class) {
	switch (storage_class) {
	case StorageClass::NONE:
		break;
	case StorageClass::TYPEDEF:
		stream << "\"typedef\"";
		break;
	case StorageClass::EXTERN:
		stream << "\"extern\"";
		break;
	case StorageClass::STATIC:
		stream << "\"static\"";
		break;
	case StorageClass::AUTO:
		stream << "\"auto\"";
		break;
	case StorageClass::REGISTER:
		stream << "\"register\"";
		break;
	}
	return stream;
}

ostream& operator<<(ostream& stream, const DeclStatementList& items) {
    stream << '[';
    for (auto i = 0; i < items.size(); ++i) {
        if (i != 0) stream << ", ";
        stream << items[i];
    }
    return stream << ']';
}
