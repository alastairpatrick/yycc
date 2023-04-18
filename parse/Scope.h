#ifndef PARSE_SCOPE_H
#define PARSE_SCOPE_H

#include "InternedString.h"

struct Declarator;

struct Scope {
    unordered_map<InternedString, Declarator*> declarators;
};

#endif
