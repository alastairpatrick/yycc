#ifndef SYMBOL_H
#define SYMBOL_H

#include "std.h"
#include "type.h"

struct SymbolScope {
    const Type* lookup_type(TypeNameKind kind, const std::string& name) const { return nullptr; }

private:

};

#endif