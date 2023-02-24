#ifndef PRINT_H
#define PRINT_H

#include "std.h"

struct Printable {
    virtual void print(ostream& stream) const = 0;
    virtual ~Printable();
};

ostream& operator<<(ostream& stream, const Printable* expr);

#endif
