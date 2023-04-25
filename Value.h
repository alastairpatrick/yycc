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
    unsigned qualifiers{};

    Value() = default;

    Value(const Type* type, LLVMValueRef llvm = nullptr)
            : kind(llvm ? ValueKind::RVALUE : ValueKind::TYPE_ONLY), llvm(llvm), type(type), qualifiers(type->qualifiers()) {
        assert(type);
    }

    Value(ValueKind kind, const Type* type, LLVMValueRef llvm)
        : kind(kind), llvm(llvm), type(type), qualifiers(type->qualifiers()) {
        assert(type);
        assert(kind == ValueKind::TYPE_ONLY || llvm);
    }

    bool is_valid() const {
        return type;
    }

    bool is_const() const {
        return kind == ValueKind::RVALUE && LLVMIsConstant(llvm);
    }

    bool is_const_integer() const{
        return kind == ValueKind::RVALUE && LLVMIsAConstantInt(llvm);
    }

    LLVMValueRef llvm_const_rvalue() const{
        assert(is_const());
        return llvm;
    }

    LLVMValueRef llvm_lvalue() const{
        assert(llvm && kind == ValueKind::LVALUE);
        return llvm;
    }

    LLVMValueRef llvm_rvalue(LLVMBuilderRef builder) const;

    Value unqualified() const {
        return bit_cast(type->unqualified());
    }

    Value bit_cast(const Type* type) const {
        auto result(*this);
        result.type = type;
        return result;
    }

    Value load(LLVMBuilderRef builder) const {
        return Value(type, llvm_rvalue(builder));
    }

    void store(LLVMBuilderRef builder, const Value& new_value) const;

private:
    LLVMValueRef llvm{};
};

#endif
