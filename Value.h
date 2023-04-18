#ifndef VALUE_H
#define VALUE_H

#include "parse/Type.h"

struct Value {
    bool is_const() const;
    bool is_const_integer() const;

    Value() = default;
    Value(const Type* type, LLVMValueRef llvm = nullptr);

    LLVMValueRef llvm{};
    const Type* type{};
};

#endif
