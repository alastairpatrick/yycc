#ifndef VALUE_H
#define VALUE_H

#include "parse/Type.h"

enum class ValueKind {
    TYPE_ONLY,
    RVALUE,
    LVALUE,
};

struct Value {
    ValueKind kind = ValueKind::TYPE_ONLY;
    const Type* type{};

    Value() = default;
    Value(const Type* type, LLVMValueRef llvm = nullptr);
    Value(ValueKind kind, const Type* type, LLVMValueRef llvm);
    bool is_const() const;
    bool is_const_integer() const;
    LLVMValueRef llvm_const_rvalue() const;
    LLVMValueRef llvm_lvalue() const;
    LLVMValueRef llvm_rvalue(LLVMBuilderRef builder) const;
    Value bit_cast(const Type* type) const;

private:
    LLVMValueRef llvm{};
};

#endif
