#ifndef VISITOR_RESOLVE_PASS_H
#define VISITOR_RESOLVE_PASS_H

#include "visitor/Visitor.h"

struct Declarator;
struct Type;

struct ResolvePass: Visitor {
    const Type* resolve(Declarator* primary);
    void compose(Declarator* primary, Declarator* secondary);

    const Type* resolve(const Type* type);
    void resolve(Statement* statement);

    virtual VisitDeclaratorOutput visit(Declarator* declarator, Entity* entity, const VisitDeclaratorInput& input) override;
    virtual VisitDeclaratorOutput visit(Declarator* declarator, TypeDef* type_def, const VisitDeclaratorInput& input) override;

    virtual VisitTypeOutput visit_default(const Type* type, const VisitTypeInput& input) override;
    virtual VisitTypeOutput visit(const PointerType* type, const VisitTypeInput& input) override;
    virtual VisitTypeOutput visit(const QualifiedType* type, const VisitTypeInput& input) override;
    virtual VisitTypeOutput visit(const UnqualifiedType* type, const VisitTypeInput& input) override;
    virtual VisitTypeOutput visit(const FunctionType* type, const VisitTypeInput& input) override;
    virtual VisitTypeOutput visit(const StructType* type, const VisitTypeInput& input) override;
    virtual VisitTypeOutput visit(const UnionType* type, const VisitTypeInput& input) override;
    VisitTypeOutput visit_structured_type(const StructuredType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const EnumType* type, const VisitTypeInput& input) override;
    virtual VisitTypeOutput visit(const TypeOfType* type, const VisitTypeInput& input) override;
    virtual VisitTypeOutput visit(const TypeDefType* type, const VisitTypeInput& input) override;
    virtual VisitTypeOutput visit(const UnresolvedArrayType* type, const VisitTypeInput& input) override;

    virtual VisitStatementOutput visit(EntityExpr* expr, const VisitStatementInput& input) override;
    virtual VisitStatementOutput visit(SizeOfExpr* expr, const VisitStatementInput& input) override;

    unordered_set<Declarator*> todo;
};

#endif

