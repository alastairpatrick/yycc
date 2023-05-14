#ifndef VALUE_H
#define VALUE_H

#include "parse/Type.h"

enum class EmitOutcome {
    TYPE,
    FOLD,
    IR,
};

// Must only be thrown if the emit outcome is FOLD.
struct FoldError {
    bool error_reported;

    FoldError(bool error_reported): error_reported(error_reported) {
    }
};

enum class ValueKind: uint8_t {
    INVALID,
    TYPE_ONLY,
    RVALUE,
    LVALUE,
};

struct BitField;

struct Value {
    ValueKind kind = ValueKind::INVALID;
    bool is_null_literal{};
    QualifierSet qualifiers{};
    const Type* type{};
    BitField* bit_field{};

    Value() = default;

    explicit Value(const Type* type, LLVMValueRef llvm = nullptr)
            : kind(llvm ? ValueKind::RVALUE : ValueKind::TYPE_ONLY), llvm(llvm), type(type), qualifiers(type->qualifiers()) {
        assert(type);
    }

    Value(ValueKind kind, const Type* type, LLVMValueRef llvm)
        : kind(kind), llvm(llvm), type(type), qualifiers(type->qualifiers()) {
        assert(type);
        assert(kind == ValueKind::TYPE_ONLY || llvm);
    }

    static Value of_null(const Type* type) {
        return Value(type, LLVMConstNull(type->llvm_type()));
    }

    static Value of_int(const IntegerType* type, unsigned long long int_value) {
        return Value(type, LLVMConstInt(type->llvm_type(), int_value, type->is_signed()));
    }

    static Value of_zero_int();

    static Value of_recover(const Type* type);

    bool is_valid() const {
        return kind != ValueKind::INVALID;
    }

    bool is_const() const {
        return kind == ValueKind::RVALUE && LLVMIsConstant(llvm);
    }

    bool is_const_integer() const{
        return kind == ValueKind::RVALUE && LLVMIsAConstantInt(llvm);
    }

    LLVMValueRef get_const() const{
        assert(is_const());
        return llvm;
    }

    LLVMValueRef get_lvalue() const{
        assert(llvm && kind == ValueKind::LVALUE);
        return llvm;
    }

    // Use Emitter::get_rvalue instead
    LLVMValueRef dangerously_get_rvalue(LLVMBuilderRef builder) const;

    LLVMValueRef get_rvalue(LLVMBuilderRef builder, EmitOutcome outcome) const;

    Value unqualified() const {
        return bit_cast(type->unqualified());
    }

    Value bit_cast(const Type* type) const {
        auto result(*this);
        result.type = type;
        return result;
    }

    void store(LLVMBuilderRef builder, const Value& new_value) const;

private:
    LLVMValueRef llvm{};
};

#endif
