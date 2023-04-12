#ifndef CODE_GEN_CONTEXT_H
#define CODE_GEN_CONTEXT_H

struct EmitContext {
    LLVMModuleRef mod;
    LLVMValueRef function;
    LLVMBuilderRef builder;
};

#endif
