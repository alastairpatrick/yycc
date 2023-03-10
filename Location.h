#ifndef LOCATION_H
#define LOCATION_H

struct Location {
    size_t line{};
    size_t column{};
    string_view filename;
};

#endif
