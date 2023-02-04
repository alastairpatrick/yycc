#ifndef PRINT_H
#define PRINT_H

#include "std.h"

struct Printable {
    virtual void print(std::ostream& stream) const = 0;
    virtual ~Printable();
};

std::ostream& operator<<(std::ostream& stream, const Printable* expr);
std::ostream& operator<<(std::ostream& stream, const string* expr);

#endif
