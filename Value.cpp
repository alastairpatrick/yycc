#include "Value.h"
#include "parse/Declaration.h"
#include "TranslationUnitContext.h"

Value Value::of_zero_int() {
    auto context = TranslationUnitContext::it;
    return Value(IntegerType::default_type(), context->zero_int);
}

Value Value::of_recover(const Type* type) {
    type = type->unqualified();

    // Too late to fail properly here.
    assert(!dynamic_cast<const FunctionType*>(type));

    if (type == &VoidType::it) {
        return Value(type);
    } else {
        return of_null(type);
    }
}

LLVMValueRef Value::dangerously_get_rvalue(LLVMBuilderRef builder) const {
    assert(llvm);
    if (kind == ValueKind::RVALUE) return llvm;

    auto value = LLVMBuildLoad2(builder, bit_field ? bit_field->storage_type : type->llvm_type(), llvm, "");
    if (qualifiers & QUALIFIER_VOLATILE) {
        LLVMSetVolatile(value, true);
    }
    if (!bit_field) return value;

    auto integer_type = dynamic_cast<const IntegerType*>(type);

    value = LLVMBuildAnd(builder, value, bit_field->mask, "");

    if (integer_type->is_signed()) {
        // value = (value << bits_to_left) >> (bits_to_left + bits_to_right)
        value = LLVMBuildAShr(builder, LLVMBuildShl(builder, value, bit_field->bits_to_left, ""), LLVMConstAdd(bit_field->bits_to_left, bit_field->bits_to_right), "");
    } else {
        // value = value >> bits_to_right
        value = LLVMBuildLShr(builder, value, bit_field->bits_to_right, "");
    }

    return LLVMBuildIntCast2(builder, value, type->llvm_type(), integer_type->is_signed(), "");
}

void Value::store(LLVMBuilderRef builder, const Value& new_value) const {
    auto value = new_value.dangerously_get_rvalue(builder);

    if (bit_field) {
        // value = (value << bits_to_right) & mask
        value = LLVMBuildIntCast2(builder, value, bit_field->storage_type, false, "");  // mask renders sign extension irrelevant
        value = LLVMBuildAnd(builder, LLVMBuildShl(builder, value, bit_field->bits_to_right, ""), bit_field->mask, "");

        // existing = (*p) & ~mask
        LLVMValueRef existing = LLVMBuildLoad2(builder, bit_field->storage_type, get_lvalue(), "");
        LLVMSetVolatile(existing, qualifiers & QUALIFIER_VOLATILE);
        existing = LLVMBuildAnd(builder, existing, LLVMBuildNot(builder, bit_field->mask, ""), "");

        // value = existing | value
        value = LLVMBuildOr(builder, value, existing, "");
    }

    LLVMSetVolatile(LLVMBuildStore(builder, value, get_lvalue()),
                    qualifiers & QUALIFIER_VOLATILE);
}

