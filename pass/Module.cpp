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

Value Module::indeterminate_bool() {
    auto context = TranslationUnitContext::it;

    if (cached_indeterminate_bool.is_valid()) return cached_indeterminate_bool;

    cached_indeterminate_bool = Value(ValueKind::LVALUE, IntegerType::of_bool(),
                                      LLVMAddGlobal(llvm_module, context->llvm_bool_type, "indeterminate"));
    return cached_indeterminate_bool;
}

TypedFunctionRef Module::lookup_intrinsic(const char* name, LLVMTypeRef* param_types, unsigned num_params) {
    auto context = TranslationUnitContext::it;

    TypedFunctionRef ref;
    auto id = LLVMLookupIntrinsicID(name, strlen(name));
    ref.function = LLVMGetIntrinsicDeclaration(llvm_module, id, param_types, num_params);
    ref.type = LLVMIntrinsicGetType(context->llvm_context, id, param_types, num_params);
    return ref;
}

void Module::middle_end_passes(const char* passes) {
    auto pass_builder_options = LLVMCreatePassBuilderOptions();
    SCOPE_EXIT {
        LLVMDisposePassBuilderOptions(pass_builder_options);
    };

    LLVMRunPasses(llvm_module, passes, g_llvm_target_machine, pass_builder_options);
}

void Module::back_end_passes() {
    char* error{};
    LLVMTargetMachineEmitToFile(g_llvm_target_machine, llvm_module, "generated.asm", LLVMAssemblyFile, &error);
    LLVMDisposeMessage(error);
}
