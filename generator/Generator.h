#ifndef GENERATOR_GENERATOR_H
#define GENERATOR_GENERATOR_H

struct Generator {
    Generator(const Generator&) = delete;
    void operator=(const Generator&) = delete;
    virtual ~Generator();

    virtual LLVMValueRef add(LLVMValueRef left, LLVMValueRef right, const char* name) = 0;
    virtual LLVMValueRef sub(LLVMValueRef left, LLVMValueRef right, const char* name) = 0;
    virtual LLVMValueRef mul(LLVMValueRef left, LLVMValueRef right, const char* name) = 0;
    virtual LLVMValueRef sdiv(LLVMValueRef left, LLVMValueRef right, const char* name) = 0;
    virtual LLVMValueRef udiv(LLVMValueRef left, LLVMValueRef right, const char* name) = 0;
    virtual LLVMValueRef fadd(LLVMValueRef left, LLVMValueRef right, const char* name) = 0;
    virtual LLVMValueRef fsub(LLVMValueRef left, LLVMValueRef right, const char* name) = 0;
    virtual LLVMValueRef fmul(LLVMValueRef left, LLVMValueRef right, const char* name) = 0;
    virtual LLVMValueRef fdiv(LLVMValueRef left, LLVMValueRef right, const char* name) = 0;
};

struct IRGenerator: Generator {
    LLVMBuilderRef builder{};

    virtual LLVMValueRef add(LLVMValueRef left, LLVMValueRef right, const char* name) override;
    virtual LLVMValueRef sub(LLVMValueRef left, LLVMValueRef right, const char* name) override;
    virtual LLVMValueRef mul(LLVMValueRef left, LLVMValueRef right, const char* name) override;
    virtual LLVMValueRef sdiv(LLVMValueRef left, LLVMValueRef right, const char* name) override;
    virtual LLVMValueRef udiv(LLVMValueRef left, LLVMValueRef right, const char* name) override;
    virtual LLVMValueRef fadd(LLVMValueRef left, LLVMValueRef right, const char* name) override;
    virtual LLVMValueRef fsub(LLVMValueRef left, LLVMValueRef right, const char* name) override;
    virtual LLVMValueRef fmul(LLVMValueRef left, LLVMValueRef right, const char* name) override;
    virtual LLVMValueRef fdiv(LLVMValueRef left, LLVMValueRef right, const char* name) override;
};

struct Evaluator: Generator {
    virtual LLVMValueRef add(LLVMValueRef left, LLVMValueRef right, const char* name) override;
    virtual LLVMValueRef sub(LLVMValueRef left, LLVMValueRef right, const char* name) override;
    virtual LLVMValueRef mul(LLVMValueRef left, LLVMValueRef right, const char* name) override;
    virtual LLVMValueRef sdiv(LLVMValueRef left, LLVMValueRef right, const char* name) override;
    virtual LLVMValueRef udiv(LLVMValueRef left, LLVMValueRef right, const char* name) override;
    virtual LLVMValueRef fadd(LLVMValueRef left, LLVMValueRef right, const char* name) override;
    virtual LLVMValueRef fsub(LLVMValueRef left, LLVMValueRef right, const char* name) override;
    virtual LLVMValueRef fmul(LLVMValueRef left, LLVMValueRef right, const char* name) override;
    virtual LLVMValueRef fdiv(LLVMValueRef left, LLVMValueRef right, const char* name) override;
};

#endif
