#include "Value.h"
#include "parse/Declaration.h"
#include "TranslationUnitContext.h"

Value Value::default_int() {
    auto context = TranslationUnitContext::it;
    return Value(IntegerType::default_type(), context->zero_int);
}

LLVMValueRef Value::llvm_rvalue(LLVMBuilderRef builder) const {
    assert(llvm);
    if (kind == ValueKind::RVALUE) return llvm;

    auto value = LLVMBuildLoad2(builder, bit_field ? bit_field->storage_type : type->llvm_type(), llvm, "");
    if (qualifiers & QUAL_VOLATILE) {
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
    auto value = new_value.llvm_rvalue(builder);

    if (bit_field) {
        // value = (value << bits_to_right) & mask
        value = LLVMBuildIntCast2(builder, value, bit_field->storage_type, false, "");  // mask renders sign extension irrelevant
        value = LLVMBuildAnd(builder, LLVMBuildShl(builder, value, bit_field->bits_to_right, ""), bit_field->mask, "");

        // existing = (*p) & ~mask
        LLVMValueRef existing = LLVMBuildLoad2(builder, bit_field->storage_type, llvm_lvalue(), "");
        LLVMSetVolatile(existing, qualifiers & QUAL_VOLATILE);
        existing = LLVMBuildAnd(builder, existing, LLVMBuildNot(builder, bit_field->mask, ""), "");

        // value = existing | value
        value = LLVMBuildOr(builder, value, existing, "");
    }

    LLVMSetVolatile(LLVMBuildStore(builder, value, llvm_lvalue()),
                    qualifiers & QUAL_VOLATILE);
}

