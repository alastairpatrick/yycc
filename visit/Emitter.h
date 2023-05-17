#ifndef VISIT_EMITTER_H
#define VISIT_EMITTER_H

#include "Module.h"
#include "parse/Type.h"
#include "Value.h"

struct EmitOptions {
    bool initialize_variables = true;
    bool emit_helpers = true;
};

const Type* get_expr_type(const Expr* expr);
Value fold_expr(const Expr* expr);
void emit_pass(Module& module, const EmitOptions& options);

#endif