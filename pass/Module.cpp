#include "Module.h"
#include "LLVM.h"
#include "parse/Type.h"
#include "TranslationUnitContext.h"

Module::Module() {
    auto context = TranslationUnitContext::it;
    llvm_module = LLVMModuleCreateWithNameInContext("my_module", context->llvm_context);
}

Module::~Module() {
    LLVMDisposeModule(llvm_module);
}

Module::DestructorPlaceholder Module::get_destructor_placeholder(const StructuredType* type) {
    auto context = TranslationUnitContext::it;

    auto it = destructor_placeholders.find(type);
    if (it != destructor_placeholders.end()) return it->second;

    DestructorPlaceholder placeholder;

    // void placeholder(T* receiver, void actual_destructor(T*), T actual_state, T default_state)
    LLVMTypeRef placeholder_params[] = {
        context->llvm_pointer_type,
        context->llvm_pointer_type,
        type->llvm_type(),
        type->llvm_type(),
    };
    placeholder.type = LLVMFunctionType(context->llvm_void_type, placeholder_params, 4, false);
    placeholder.function = LLVMAddFunction(llvm_module, "destructor_placeholder", placeholder.type);

    destructor_placeholders[type] = placeholder;
    destructor_placeholder_functions.insert(placeholder.function);

    return placeholder;
}

void Module::analysis_pass() {
    auto pass_builder_options = LLVMCreatePassBuilderOptions();
    SCOPE_EXIT {
        LLVMDisposePassBuilderOptions(pass_builder_options);
    };

    LLVMRunPasses(llvm_module, "instcombine,sccp", g_llvm_target_machine, pass_builder_options);
}

void Module::back_end_pass() {
    auto pass_builder_options = LLVMCreatePassBuilderOptions();

    LLVMRunPasses(llvm_module, "default<O0>", g_llvm_target_machine, pass_builder_options);

    LLVMDisposePassBuilderOptions(pass_builder_options);

    char* error{};
    LLVMTargetMachineEmitToFile(g_llvm_target_machine, llvm_module, "generated.asm", LLVMAssemblyFile, &error);
    LLVMDisposeMessage(error);
}
