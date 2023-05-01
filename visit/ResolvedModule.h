#ifndef VISIT_RESOLVED_MODULE_H
#define VISIT_RESOLVED_MODULE_H

#include "parse/Scope.h"

struct ResolvedModule {
    Scope* file_scope;
    vector<Scope*> type_scopes;
};

#endif
