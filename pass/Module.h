#ifndef VISIT_RESOLVED_MODULE_H
#define VISIT_RESOLVED_MODULE_H

#include "parse/Scope.h"
#include "parse/Type.h"
#include "Value.h"

struct EmitOptions {
    bool initialize_variables = true;
    bool emit_helpers = true;
    bool emit_parameter_attributes = true;
};

const Type* get_expr_type(const Expr* expr);
Value fold_expr(const Expr* expr, ValueKind kind = ValueKind::RVALUE);

struct TypedFunctionRef {
    LLVMTypeRef type{};
    LLVMValueRef function{};

    LLVMValueRef call(LLVMBuilderRef builder, LLVMValueRef* args, unsigned num_args) {
        return LLVMBuildCall2(builder, type, function, args, num_args, "");
    }
};

struct Module {
    const EmitOptions& options;
    Scope* file_scope;
    vector<Scope*> type_scopes;

    LLVMModuleRef llvm_module{};
    LLVMBuilderRef builder{};

    // Maps from a constant to a constant global initialized with that constant. Intended only to pool strings.
    // Note that LLVM internally performs constant uniqueing, ensuring that constants with the same type and
    // value are the same instance.
    unordered_map<LLVMValueRef, LLVMValueRef> reified_constants;

    unordered_map<const Type*, LLVMValueRef> default_values;

    LLVMTypeRef destructor_wrapper_type{};
    unordered_map<const StructuredType*, TypedFunctionRef> destructor_wrappers;

    unordered_set<LLVMValueRef> destructor_placeholder_functions;

    Module(const EmitOptions& options);
    ~Module();

    TypedFunctionRef destructor_wrapper(const StructuredType* type, LLVMValueRef default_value);

    Value indeterminate_bool();
    TypedFunctionRef lookup_intrinsic(const char* name, LLVMTypeRef* param_types, unsigned num_params);
    
    void call_assume_intrinsic(LLVMBuilderRef builder, LLVMValueRef true_value);
    void call_expect_i1_intrinsic(LLVMBuilderRef builder, LLVMValueRef actual_value, LLVMValueRef expected_value);
    Value call_is_constant_intrinsic(LLVMBuilderRef builder, LLVMValueRef value, LLVMTypeRef type);
    void call_sideeffect_intrinsic(LLVMBuilderRef builder);

    unsigned get_enum_attribute_kind(const char* name);
    LLVMAttributeRef create_enum_attribute(const char* name);
    LLVMAttributeRef dereferenceable_attribute(uint64_t size);
    LLVMAttributeRef noalias_attribute();
    LLVMAttributeRef nocapture_attribute();
    LLVMAttributeRef nonnull_attribute();
    LLVMAttributeRef noundef_attribute();
    LLVMAttributeRef readonly_attribute();

    void resolve_pass(const vector<Declaration*>& declarations, Scope& file_scope);
    void entity_pass();
    void emit_pass();
    void middle_end_passes(const char* passes);
    void substitution_pass();
    void back_end_passes();

private:
    Value cached_indeterminate_bool{};
    unsigned cached_dereferenceable_kind{};
    LLVMAttributeRef cached_noalias_attribute{};
    LLVMAttributeRef cached_nocapture_attribute{};
    LLVMAttributeRef cached_nonnull_attribute{};
    LLVMAttributeRef cached_noundef_attribute{};
    LLVMAttributeRef cached_readonly_attribute{};
};

#endif
