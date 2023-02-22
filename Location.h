#ifndef LOCATION_H
#define LOCATION_H

#include "std.h"

struct Location {
    size_t line = 0;
    size_t column = 0;
    const char* file = nullptr;
};

#endif
