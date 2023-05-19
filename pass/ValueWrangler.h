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

struct ConvertTypeResult {
    Value value;
    ConvKind conv_kind = ConvKind::IMPLICIT;

    ConvertTypeResult() = default;
    explicit ConvertTypeResult(Value value, ConvKind kind = ConvKind::IMPLICIT): value(value), conv_kind(kind) {}
    explicit ConvertTypeResult(const Type* type, LLVMValueRef value = nullptr, ConvKind kind = ConvKind::IMPLICIT): value(type, value), conv_kind(kind) {}
};

struct ValueWrangler: TypeVisitor {
    Module* module{};
    LLVMBuilderRef builder{};
    LLVMBuilderRef temp_builder{};
    EmitOutcome outcome{};

    ValueWrangler(Module* module, EmitOutcome outcome);
    ~ValueWrangler();

    ConvKind check_pointer_conversion(const Type* source_base_type, const Type* dest_base_type);
    ConvertTypeResult convert_to_type(const Value& value, const Type* dest_type, ConvKind kind, const Location& location);
    LLVMValueRef get_address(const Value &value);
    LLVMValueRef get_value(const Value &value, const Location& location, bool for_move_expr = false);
    void store(const Value& dest, LLVMValueRef source_rvalue, const Location& location);
    void position_temp_builder(LLVMBasicBlockRef entry_block);
    void make_addressable(Value& value, LLVMBasicBlockRef entry_block);
    Value allocate_auto_storage(const Type* type, const char* name, LLVMBasicBlockRef entry_block);

private:
    Value value;
    Location location;
    ConvertTypeResult result;

    virtual const Type* visit(const ResolvedArrayType* dest_type) override;
    virtual const Type* visit(const PointerType* dest_type) override;
    virtual const Type* visit(const IntegerType* dest_type) override;
    virtual const Type* visit(const FloatingPointType* dest_type) override;
    virtual const Type* visit(const EnumType* dest_type) override;
    virtual const Type* visit(const VoidType* dest_type) override;

    void convert_array_to_pointer();
    void convert_enum_to_int();
    LLVMValueRef get_value(const Value &value);
};


#endif
