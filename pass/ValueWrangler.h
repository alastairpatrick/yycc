#ifndef VISIT_VALUE_WRANGLER_H
#define VISIT_VALUE_WRANGLER_H

#include "TypeVisitor.h"
#include "Value.h"

struct Module;

enum class ConvKind {
    IMPLICIT,
    C_IMPLICIT, // conversions that need not be explicit in C
    EXPLICIT,
};

struct ExprValue: Value {
    const LocationNode* node{};

    ExprValue() = default;
    ExprValue(const ExprValue&) = default;
    ExprValue(const Value& value, const LocationNode* node): Value(value), node(node) {}
    ExprValue(const Type* type, const LocationNode* node): Value(type), node(node) {}
    ExprValue(const Type* type, LLVMValueRef llvm, const LocationNode* node): Value(type, llvm), node(node) {}
    ExprValue(ValueKind kind, const Type* type, LLVMValueRef llvm, const LocationNode* node): Value(kind, type, llvm), node(node) {}

    ExprValue unqualified() const {
        return ExprValue(Value::unqualified(), node);
    }

    ExprValue bit_cast(const Type* type) const {
        return ExprValue(Value::bit_cast(type), node);
    }
};

template <typename T, typename U>
inline const T* unqualified_type_cast(const U* type) {
    assert(type->qualifiers() == 0);
    return dynamic_cast<const T*>(type);
}

struct ValueWrangler: TypeVisitor {
    Module* module{};
    LLVMBuilderRef builder{};
    LLVMBuilderRef temp_builder{};
    EmitOutcome outcome{};
    LLVMBasicBlockRef entry_block{};

    ValueWrangler(Module* module, EmitOutcome outcome);
    ~ValueWrangler();

    ConvKind check_pointer_conversion(const Type* source_base_type, const Type* dest_base_type);
    ExprValue convert_to_type(const ExprValue& value, const Type* dest_type, ConvKind kind);
    LLVMValueRef get_address(const Value &value);
    LLVMValueRef get_value(const ExprValue &value, bool for_move_expr = false);
    void store(const Value& dest, LLVMValueRef source_rvalue, const Location& assignment_location);
    void position_temp_builder();
    void make_addressable(Value& value);
    Value allocate_auto_storage(const Type* type, const char* name);

    void call_assume_intrinsic(LLVMValueRef true_value);
    void call_expect_i1_intrinsic(LLVMValueRef actual_value, LLVMValueRef expected_value);
    Value call_is_constant_intrinsic(const Value& value);
    void call_sideeffect_intrinsic();

private:
    ExprValue value;
    Value result;
    ConvKind conv_kind;

    virtual const Type* visit(const ResolvedArrayType* dest_type) override;
    virtual const Type* visit(const PointerType* dest_type) override;
    virtual const Type* visit(const IntegerType* dest_type) override;
    virtual const Type* visit(const FloatingPointType* dest_type) override;
    virtual const Type* visit(const EnumType* dest_type) override;
    virtual const Type* visit(const VoidType* dest_type) override;

    void convert_array_to_pointer();
    void convert_enum_to_int();
    LLVMValueRef get_value_internal();
};


#endif
