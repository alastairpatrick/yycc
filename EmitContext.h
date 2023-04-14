#ifndef EMIT_CONTEXT_H
#define EMIT_CONTEXT_H

struct ResolutionContext;

enum class EmitOutcome {
    TYPE,
    FOLD,
    IR,
};

struct EmitContext {
    EmitOutcome outcome = EmitOutcome::TYPE;

    LLVMTargetRef target{};
    LLVMTargetMachineRef target_machine{};
    LLVMTargetDataRef target_data{};

    LLVMModuleRef mod{};
    LLVMValueRef function{};
    LLVMBuilderRef builder{};
};

#endif
