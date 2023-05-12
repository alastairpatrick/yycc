#ifndef VISIT_TYPE_CONVERTER_H
#define VISIT_TYPE_CONVERTER_H

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

ConvKind check_pointer_conversion(const Type* source_base_type, const Type* dest_base_type);

ConvertTypeResult convert_to_type(const Value& value, const Type* dest_type, Module* module, LLVMBuilderRef builder, EmitOutcome outcome);

#endif
