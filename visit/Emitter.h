#ifndef VISIT_EMITTER_H
#define VISIT_EMITTER_H

#include "Visitor.h"

struct EmitOptions {
    bool initialize_variables = true;
};

const Type* get_expr_type(const Expr* expr);
Value fold_expr(const Expr* expr, unsigned long long error_value = 0);
LLVMModuleRef emit_pass(const ASTNodeVector& nodes, const EmitOptions& options);

#endif