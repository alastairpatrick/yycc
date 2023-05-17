#ifndef VISIT_RESOLVE_PASS_H
#define VISIT_RESOLVE_PASS_H

#include "parse/Declaration.h"
#include "parse/Scope.h"
#include "Module.h"

void resolve_pass(Module& module, const vector<Declaration*>& declarations, Scope& file_scope);

#endif
