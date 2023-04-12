#include "Generator.h"

Generator::~Generator() {
}

LLVMValueRef IRGenerator::add(LLVMValueRef left, LLVMValueRef right, const char* name) {
    return LLVMBuildAdd(builder, left, right, name);
}

LLVMValueRef Evaluator::add(LLVMValueRef left, LLVMValueRef right, const char* name) {
    return LLVMConstAdd(left, right);
}

LLVMValueRef IRGenerator::sub(LLVMValueRef left, LLVMValueRef right, const char* name) {
    return LLVMBuildSub(builder, left, right, name);
}

LLVMValueRef Evaluator::sub(LLVMValueRef left, LLVMValueRef right, const char* name) {
    return LLVMConstSub(left, right);
}

LLVMValueRef IRGenerator::mul(LLVMValueRef left, LLVMValueRef right, const char* name) {
    return LLVMBuildMul(builder, left, right, name);
}

LLVMValueRef Evaluator::mul(LLVMValueRef left, LLVMValueRef right, const char* name) {
    return LLVMConstMul(left, right);
}

LLVMValueRef IRGenerator::sdiv(LLVMValueRef left, LLVMValueRef right, const char* name) {
    return LLVMBuildSDiv(builder, left, right, name);
}

LLVMValueRef Evaluator::sdiv(LLVMValueRef left, LLVMValueRef right, const char* name) {
    return LLVMSDiv(left, right);
}
