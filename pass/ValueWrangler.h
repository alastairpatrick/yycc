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

struct ConvertTypeResult {
    Value value;
    ConvKind conv_kind = ConvKind::IMPLICIT;

    ConvertTypeResult() = default;
    explicit ConvertTypeResult(Value value, ConvKind kind = ConvKind::IMPLICIT): value(value), conv_kind(kind) {}
    explicit ConvertTypeResult(const Type* type, LLVMValueRef value = nullptr, ConvKind kind = ConvKind::IMPLICIT): value(type, value), conv_kind(kind) {}
};

struct ValueWrangler: TypeVisitor {
private:
    Module* module{};
    LLVMBuilderRef builder{};
    EmitOutcome outcome{};
    Value value;

    Location location;
    ConvertTypeResult result;

public:
    ValueWrangler(Module* module, LLVMBuilderRef builder, EmitOutcome outcome);

    ConvKind check_pointer_conversion(const Type* source_base_type, const Type* dest_base_type);
    ConvertTypeResult convert_to_type(const Value& value, const Type* dest_type, const Location& location);
    LLVMValueRef get_rvalue(const Value &value, const Location& location, bool for_move_expr = false);

    virtual const Type* visit(const ResolvedArrayType* dest_type) override;
    virtual const Type* visit(const PointerType* dest_type) override;
    virtual const Type* visit(const IntegerType* dest_type) override;
    virtual const Type* visit(const FloatingPointType* dest_type) override;
    virtual const Type* visit(const EnumType* dest_type) override;
    virtual const Type* visit(const VoidType* dest_type) override;

private:
    void convert_array_to_pointer();
    void convert_enum_to_int();
    LLVMValueRef get_rvalue(const Value &value);
};


#endif
