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

void Module::call_assume_intrinsic(LLVMBuilderRef builder, LLVMValueRef true_value) {
    auto function = lookup_intrinsic("llvm.assume", nullptr, 0);

    LLVMValueRef args[] = {
        true_value,
    };
    function.call(builder, args, std::size(args));
}

void Module::call_expect_i1_intrinsic(LLVMBuilderRef builder, LLVMValueRef actual_value, LLVMValueRef expected_value) {
    auto context = TranslationUnitContext::it;

    LLVMTypeRef param_types[] = {
        context->llvm_bool_type,
    };
    auto function = lookup_intrinsic("llvm.expect", param_types, std::size(param_types));

    LLVMValueRef args[] = {
        actual_value,
        expected_value,
    };
    function.call(builder, args, std::size(args));
}

Value Module::call_is_constant_intrinsic(LLVMBuilderRef builder, LLVMValueRef value, LLVMTypeRef type) {
    auto function = lookup_intrinsic("llvm.is.constant", &type, 1);
    return Value(IntegerType::of_bool(), function.call(builder, &value, 1));
}

void Module::call_sideeffect_intrinsic(LLVMBuilderRef builder) {
    auto function = lookup_intrinsic("llvm.sideeffect", nullptr, 0);
    function.call(builder, nullptr, 0);
}

LLVMAttributeRef Module::create_enum_attribute(const char* name) {
    auto context = TranslationUnitContext::it;
    auto kind = LLVMGetEnumAttributeKindForName(name, strlen(name));
    return LLVMCreateEnumAttribute(context->llvm_context, kind, 0);
}

LLVMAttributeRef Module::nocapture_attribute() {
    if (cached_nocapture_attribute) return cached_nocapture_attribute;
    return cached_nocapture_attribute = create_enum_attribute("nocapture");
}

LLVMAttributeRef Module::nonnull_attribute() {
    if (cached_nonnull_attribute) return cached_nonnull_attribute;
    return cached_nonnull_attribute = create_enum_attribute("nonnull");
}

LLVMAttributeRef Module::noundef_attribute() {
    if (cached_nonnull_attribute) return cached_nonnull_attribute;
    return cached_nonnull_attribute = create_enum_attribute("noundef");
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
