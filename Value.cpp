#include "Value.h"

Value::Value(const Type* type, LLVMValueRef llvm)
    : llvm(llvm), type(type) {
}

bool Value::is_const() const {
    return llvm && LLVMIsConstant(llvm);
}

bool Value::is_const_integer() const {
    return llvm && LLVMIsAConstantInt(llvm);
}
