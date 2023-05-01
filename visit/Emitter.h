#ifndef VISIT_EMITTER_H
#define VISIT_EMITTER_H

#include "Visitor.h"

struct EmitOptions {
    bool initialize_variables = true;
};

const Type* get_expr_type(const Expr* expr);
Value fold_expr(const Expr* expr);
LLVMModuleRef emit_pass(const vector<Declaration*>& declarations, const EmitOptions& options);

#endif