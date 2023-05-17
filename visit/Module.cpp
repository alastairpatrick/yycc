#include "Module.h"
#include "parse/Type.h"
#include "TranslationUnitContext.h"

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
