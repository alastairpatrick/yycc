#ifndef VISIT_RESOLVE_PASS_H
#define VISIT_RESOLVE_PASS_H

#include "parse/Declaration.h"
#include "parse/Scope.h"
#include "ResolvedModule.h"

ResolvedModule resolve_pass(const vector<Declaration*>& declarations, Scope& file_scope);

#endif
