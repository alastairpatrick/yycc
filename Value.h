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
    struct {
        bool is_null_literal  : 1 = false;
        bool has_address      : 1 = false;
    };
    QualifierSet qualifiers{};
    const Type* type{};
    BitField* bit_field{};

    Value() = default;

    explicit Value(const Type* type)
        : kind(ValueKind::TYPE_ONLY), type(type), qualifiers(type->qualifiers()) {
        assert(type);
    }

    Value(const Type* type, LLVMValueRef llvm)
        : kind(ValueKind::RVALUE), llvm(llvm), type(type), qualifiers(type->qualifiers()) {
        assert(type);
    }

    Value(ValueKind kind, const Type* type, LLVMValueRef llvm)
        : kind(kind), llvm(llvm), type(type), qualifiers(type->qualifiers()) {
        assert(type);
        assert(llvm);
        has_address = kind == ValueKind::LVALUE;
    }

    static Value of_null(const Type* type) {
        return Value(type, LLVMConstNull(type->llvm_type()));
    }

    static Value of_int(const IntegerType* type, unsigned long long int_value) {
        return Value(type, LLVMConstInt(type->llvm_type(), int_value, type->is_signed()));
    }

    static Value of_size(unsigned long long i) {
        auto type = IntegerType::of_size(IntegerSignedness::UNSIGNED);
        return of_int(type, i);
    }

    static Value of_zero_int();

    static Value of_recover(const Type* type);

    bool is_valid() const {
        return kind != ValueKind::INVALID;
    }

    bool is_const() const {
        return !has_address && LLVMIsConstant(llvm);
    }

    bool is_const_integer() const{
        return !has_address && LLVMIsAConstantInt(llvm);
    }

    LLVMValueRef get_const() const{
        assert(is_const());
        return llvm;
    }

    // Use Emitter::get_lvalue instead
    LLVMValueRef dangerously_get_address() const{
        assert(llvm);
        assert(has_address);
        assert(!bit_field);
        return llvm;
    }

    // Use ValueWrangler::get_rvalue instead
    LLVMValueRef dangerously_get_value(LLVMBuilderRef builder, EmitOutcome outcome) const;

    // Use ValueWrangler::store instead
    void dangerously_store(LLVMBuilderRef builder, LLVMValueRef new_rvalue) const;

    void make_addressable(LLVMBuilderRef alloc_builder, LLVMBuilderRef store_builder);

    Value unqualified() const {
        return bit_cast(type->unqualified());
    }

    Value bit_cast(const Type* type) const {
        auto result(*this);
        result.type = type;
        return result;
    }

private:
    LLVMValueRef llvm{};
};

#endif
