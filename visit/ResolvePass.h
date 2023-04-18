#ifndef VISIT_RESOLVE_PASS_H
#define VISIT_RESOLVE_PASS_H

#include "parse/ASTNode.h"
#include "parse/Scope.h"

void resolve_pass(const Scope& scope, const ASTNodeVector& nodes);

#endif
