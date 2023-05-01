#ifndef VISIT_RESOLVE_PASS_H
#define VISIT_RESOLVE_PASS_H

#include "parse/ASTNode.h"
#include "parse/Type.h"

struct ResolvedModule {
    Scope* file_scope;
    vector<Scope*> type_scopes;
};

ResolvedModule resolve_pass(const vector<Declaration*>& declarations, Scope& file_scope);

#endif
