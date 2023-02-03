#include "std.h"

#include "print.h"

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
