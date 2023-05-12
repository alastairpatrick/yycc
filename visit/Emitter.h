#ifndef VISIT_EMITTER_H
#define VISIT_EMITTER_H

#include "ResolvedModule.h"
#include "parse/Type.h"
#include "Value.h"

struct Module {
    LLVMModuleRef llvm_module{};

    // Maps from a constant to a constant global initialized with that constant. Intended only to pool strings.
    // Note that LLVM internally performs constant uniqueing, ensuring that constants with the same type and
    // value are the same instance.
    unordered_map<LLVMValueRef, LLVMValueRef> reified_constants;
};

struct EmitOptions {
    bool initialize_variables = true;
};

const Type* get_expr_type(const Expr* expr);
Value fold_expr(const Expr* expr);
LLVMModuleRef emit_pass(const ResolvedModule& resolved_module, const EmitOptions& options);

#endif