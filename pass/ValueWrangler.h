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
    Value convert_to_type(const Value& value, const Type* dest_type, ConvKind kind, const Location& location);
    LLVMValueRef get_address(const Value &value);
    LLVMValueRef get_value(const Value &value, const Location& location, bool for_move_expr = false);
    void store(const Value& dest, LLVMValueRef source_rvalue, const Location& location);
    void position_temp_builder();
    void make_addressable(Value& value);
    Value allocate_auto_storage(const Type* type, const char* name);

    void call_sideeffect_intrinsic();
    Value call_is_constant_intrinsic(const Value& value, const Location& location);

private:
    Value value;
    Location location;
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
    LLVMValueRef get_value_internal(const Value &value);
};


#endif
