#include "Value.h"

Value::Value(const Type* type, LLVMValueRef llvm)
    : kind(llvm ? ValueKind::RVALUE : ValueKind::TYPE_ONLY), llvm(llvm), type(type), qualifiers(type->qualifiers()) {
    assert(type);
}

Value::Value(ValueKind kind, const Type* type, LLVMValueRef llvm)
    : kind(kind), llvm(llvm), type(type), qualifiers(type->qualifiers()) {
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
        auto load = LLVMBuildLoad2(builder, type->llvm_type(), llvm, "");
        if (qualifiers & QUAL_VOLATILE) {
            LLVMSetVolatile(load, true);
        }
        return load;
    }
}

Value Value::unqualified() const {
    return bit_cast(type->unqualified());
}

Value Value::bit_cast(const Type* type) const {
    auto result(*this);
    result.type = type;
    return result;
}

Value Value::load(LLVMBuilderRef builder) const {
    return Value(type, llvm_rvalue(builder));
}

void Value::store(LLVMBuilderRef builder, const Value& new_value) const {
    LLVMSetVolatile(LLVMBuildStore(builder, new_value.llvm_rvalue(builder), llvm_lvalue()),
                    qualifiers & QUAL_VOLATILE);
}

