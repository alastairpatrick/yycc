#ifndef PRINTABLE_H
#define PRINTABLE_H

struct Printable {
    virtual void print(ostream& stream) const = 0;
    virtual ~Printable();
};

ostream& operator<<(ostream& stream, const Printable* expr);

#endif
