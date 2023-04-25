#include "Value.h"
#include "TranslationUnitContext.h"

Value Value::default_int() {
    auto context = TranslationUnitContext::it;
    return Value(IntegerType::default_type(), context->zero_int);
}

LLVMValueRef Value::llvm_rvalue(LLVMBuilderRef builder) const {
    assert(llvm);
    if (kind == ValueKind::RVALUE) {
        return llvm;
    } else {
        auto load = LLVMBuildLoad2(builder, type->llvm_type(), llvm, "");
        if (qualifiers & QUAL_VOLATILE) {
            LLVMSetVolatile(load, true);
        }
        return load;
    }
}

void Value::store(LLVMBuilderRef builder, const Value& new_value) const {
    LLVMSetVolatile(LLVMBuildStore(builder, new_value.llvm_rvalue(builder), llvm_lvalue()),
                    qualifiers & QUAL_VOLATILE);
}

