#ifndef VISITOR_EMITTER_H
#define VISITOR_EMITTER_H

#include "Visitor.h"

const Type* get_expr_type(const Expr* expr);
Value fold_expr(const Expr* expr, unsigned long long error_value = 0);
LLVMModuleRef emit_pass(const ASTNodeVector& nodes);

#endif