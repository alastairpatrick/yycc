#ifndef VISITOR_RESOLVE_PASS_H
#define VISITOR_RESOLVE_PASS_H

#include "visitor/Visitor.h"

struct Declarator;
struct Type;

struct ResolvePass: Visitor {
    const Type* resolve(Declarator* primary);
    void compose(Declarator* primary, Declarator* secondary);

    const Type* resolve(const Type* type);

    virtual VisitDeclaratorOutput visit(Declarator* declarator, Entity* entity, const VisitDeclaratorInput& input) override;
    virtual VisitDeclaratorOutput visit(Declarator* declarator, TypeDef* type_def, const VisitDeclaratorInput& input) override;

    virtual VisitTypeOutput visit_default(const Type* type, const VisitTypeInput& input) override;

    unordered_set<Declarator*> todo;
};

#endif

