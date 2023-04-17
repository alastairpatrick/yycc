#ifndef VISITOR_RESOLVE_PASS_H
#define VISITOR_RESOLVE_PASS_H

#include "parser/ASTNode.h"
#include "parser/Scope.h"

void resolve_pass(const Scope& scope, const ASTNodeVector& nodes);

#endif
