#ifndef VISITOR_VISITOR_H
#define VISITOR_VISITOR_H

#include "parser/ArrayType.h"
#include "parser/Constant.h"
#include "parser/Expr.h"
#include "parser/Statement.h"
#include "parser/Type.h"

struct VisitDeclaratorInput {
    Declarator* secondary{};

    VisitDeclaratorInput() = default;
    explicit VisitDeclaratorInput(Declarator* secondary): secondary(secondary) {}
};

struct VisitDeclaratorOutput {
};

struct VisitTypeInput {
};

struct VisitTypeOutput {
    const Type* type{};

    VisitTypeOutput() = default;
    explicit VisitTypeOutput(const Type* type): type(type) {}
};

struct VisitStatementInput {
};

struct VisitStatementOutput {
};

struct Visitor {
    virtual VisitDeclaratorOutput visit_default(Declarator* declarator, const VisitDeclaratorInput& input);
    virtual VisitDeclaratorOutput visit(Declarator* declarator, Entity* entity, const VisitDeclaratorInput& input);
    virtual VisitDeclaratorOutput visit(Declarator* declarator, TypeDef* type_def, const VisitDeclaratorInput& input);
    virtual VisitDeclaratorOutput visit(Declarator* declarator, EnumConstant* enum_constant, const VisitDeclaratorInput& input);

    virtual VisitTypeOutput visit_default(const Type* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const EnumType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const FloatingPointType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const FunctionType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const IntegerType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const PointerType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const QualifiedType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const ResolvedArrayType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const StructType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const TypeDefType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const TypeOfType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const VoidType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const UnboundType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const UnionType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const UniversalType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const UnqualifiedType* type, const VisitTypeInput& input);
    virtual VisitTypeOutput visit(const UnresolvedArrayType* type, const VisitTypeInput& input);

    virtual VisitStatementOutput visit_default(Statement* statement, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(CompoundStatement* statement, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(ReturnStatement* statement, const VisitStatementInput& input);

    virtual VisitStatementOutput visit_default(Expr* expr, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(BinaryExpr* expr, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(ConditionExpr* expr, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(EntityExpr* expr, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(InitializerExpr* expr, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(SizeOfExpr* expr, const VisitStatementInput& input);

    virtual VisitStatementOutput visit(IntegerConstant* constant, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(FloatingPointConstant* constant, const VisitStatementInput& input);
    virtual VisitStatementOutput visit(StringConstant* constant, const VisitStatementInput& input);

};

#endif

