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

    Module();
    ~Module();

    DestructorPlaceholder get_destructor_placeholder(const StructuredType* type);

    void resolve_pass(const vector<Declaration*>& declarations, Scope& file_scope);
    void entity_pass();
    void emit_pass(const EmitOptions& options);
    void analysis_pass();
    void post_analysis_pass();
    void back_end_pass();
};

#endif
