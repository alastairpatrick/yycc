#ifndef VISIT_RESOLVE_PASS_H
#define VISIT_RESOLVE_PASS_H

#include "parse/ASTNode.h"
#include "parse/Scope.h"
#include "parse/Type.h"

struct ResolvePassResult {
    vector<const TagType*> tag_types;
};

ResolvePassResult resolve_pass(const unordered_set<Declarator*>& declarators, const ASTNodeVector& nodes);

#endif
