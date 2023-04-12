#ifndef CODE_GEN_CONTEXT_H
#define CODE_GEN_CONTEXT_H

enum class EmitOutcome {
    TYPE,
    FOLD,
    IR,
};

struct EmitContext {
    EmitOutcome outcome = EmitOutcome::TYPE;
    LLVMModuleRef mod{};
    LLVMValueRef function{};
    LLVMBuilderRef builder{};
};

#endif
