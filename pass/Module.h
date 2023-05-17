#ifndef VISIT_RESOLVED_MODULE_H
#define VISIT_RESOLVED_MODULE_H

#include "parse/Scope.h"

struct Module {
    Scope* file_scope;
    vector<Scope*> type_scopes;

    LLVMModuleRef llvm_module{};

    // Maps from a constant to a constant global initialized with that constant. Intended only to pool strings.
    // Note that LLVM internally performs constant uniqueing, ensuring that constants with the same type and
    // value are the same instance.
    unordered_map<LLVMValueRef, LLVMValueRef> reified_constants;

    struct DestructorPlaceholder {
        LLVMTypeRef type;
        LLVMValueRef function;
    };

    unordered_map<const StructuredType*, DestructorPlaceholder> destructor_placeholders;
    unordered_set<LLVMValueRef> destructor_placeholder_functions;

    DestructorPlaceholder get_destructor_placeholder(const StructuredType* type);
};

#endif
