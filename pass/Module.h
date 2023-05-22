#ifndef VISIT_RESOLVED_MODULE_H
#define VISIT_RESOLVED_MODULE_H

#include "parse/Scope.h"
#include "parse/Type.h"
#include "Value.h"

struct EmitOptions {
    bool initialize_variables = true;
    bool emit_helpers = true;
};

const Type* get_expr_type(const Expr* expr);
Value fold_expr(const Expr* expr);

struct TypedFunctionRef {
    LLVMTypeRef type{};
    LLVMValueRef function{};

    LLVMValueRef call(LLVMBuilderRef builder, LLVMValueRef* args, unsigned num_args) {
        return LLVMBuildCall2(builder, type, function, args, num_args, "");
    }
};

struct Module {
    Scope* file_scope;
    vector<Scope*> type_scopes;

    LLVMModuleRef llvm_module{};

    // Maps from a constant to a constant global initialized with that constant. Intended only to pool strings.
    // Note that LLVM internally performs constant uniqueing, ensuring that constants with the same type and
    // value are the same instance.
    unordered_map<LLVMValueRef, LLVMValueRef> reified_constants;

    unordered_set<LLVMValueRef> destructor_placeholder_functions;

    Module();
    ~Module();

    Value indeterminate_bool();
    TypedFunctionRef lookup_intrinsic(const char* name, LLVMTypeRef* param_types, unsigned num_params);

    LLVMAttributeRef nocapture_attribute();

    void resolve_pass(const vector<Declaration*>& declarations, Scope& file_scope);
    void entity_pass();
    void emit_pass(const EmitOptions& options);
    void middle_end_passes(const char* passes);
    void substitution_pass();
    void back_end_passes();

private:
    Value cached_indeterminate_bool{};
    LLVMAttributeRef cached_nocapture_attribute{};
};

#endif
