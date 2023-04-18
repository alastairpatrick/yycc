#include "Value.h"

Value::Value(const Type* type, LLVMValueRef llvm)
    : kind(llvm ? ValueKind::RVALUE : ValueKind::TYPE_ONLY), llvm(llvm), type(type) {
    assert(type);
}

Value::Value(ValueKind kind, const Type* type, LLVMValueRef llvm)
    : kind(kind), llvm(llvm), type(type) {
    assert(type);
    assert(kind == ValueKind::TYPE_ONLY || llvm);
}

bool Value::is_const() const {
    return kind == ValueKind::RVALUE && LLVMIsConstant(llvm);
}

bool Value::is_const_integer() const {
    return kind == ValueKind::RVALUE && LLVMIsAConstantInt(llvm);
}

LLVMValueRef Value::llvm_const_rvalue() const {
    assert(is_const());
    return llvm;
}

LLVMValueRef Value::llvm_lvalue() const {
    assert(llvm && kind == ValueKind::LVALUE);
    return llvm;
}

LLVMValueRef Value::llvm_rvalue(LLVMBuilderRef builder) const {
    assert(llvm);
    if (kind == ValueKind::RVALUE) {
        return llvm;
    } else {
        return LLVMBuildLoad2(builder, type->llvm_type(), llvm, "");
    }
}
