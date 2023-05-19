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

LLVMValueRef Value::dangerously_get_value(LLVMBuilderRef builder, EmitOutcome outcome) const {
    assert(kind != ValueKind::INVALID);
    assert(type != &VoidType::it);
    assert(!dynamic_cast<const FunctionType*>(type));
    assert(llvm);

    if (outcome != EmitOutcome::IR) {
        // If outcome is TYPE, something ought to have earlied out before control flow got here.
        assert(outcome == EmitOutcome::FOLD);

        if (is_const()) {
            return get_const();
        } else {
            throw FoldError(false);
        }
    }

    if (!has_address) return llvm;

    auto value = LLVMBuildLoad2(builder, bit_field ? bit_field->storage_type : type->llvm_type(), llvm, "");
    if (qualifiers & QUALIFIER_VOLATILE) {
        LLVMSetVolatile(value, true);
    }

    if (bit_field) {
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

    return value;
}

void Value::dangerously_store(LLVMBuilderRef builder, LLVMValueRef new_rvalue) const {
    assert(llvm && has_address);

    if (bit_field) {
        // value = (value << bits_to_right) & mask
        new_rvalue = LLVMBuildIntCast2(builder, new_rvalue, bit_field->storage_type, false, "");  // mask renders sign extension irrelevant
        new_rvalue = LLVMBuildAnd(builder, LLVMBuildShl(builder, new_rvalue, bit_field->bits_to_right, ""), bit_field->mask, "");

        // existing = (*p) & ~mask
        LLVMValueRef existing = LLVMBuildLoad2(builder, bit_field->storage_type, llvm, "");
        LLVMSetVolatile(existing, qualifiers & QUALIFIER_VOLATILE);
        existing = LLVMBuildAnd(builder, existing, LLVMBuildNot(builder, bit_field->mask, ""), "");

        // value = existing | value
        new_rvalue = LLVMBuildOr(builder, new_rvalue, existing, "");
    }

    LLVMSetVolatile(LLVMBuildStore(builder, new_rvalue, llvm),
                    qualifiers & QUALIFIER_VOLATILE);
}

void Value::make_addressable(LLVMBuilderRef alloc_builder, LLVMBuilderRef store_builder) {
    if (has_address) return;

    auto storage = LLVMBuildAlloca(alloc_builder, type->llvm_type(), "");
    LLVMBuildStore(store_builder, llvm, storage);
    llvm = storage;
    has_address = true;
}
