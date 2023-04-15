#include "Visitor.h"


VisitDeclaratorOutput Visitor::visit_default(Declarator* declarator, const VisitDeclaratorInput& input) {
    return VisitDeclaratorOutput();
}

VisitDeclaratorOutput Visitor::visit(Declarator* declarator, Entity* entity, const VisitDeclaratorInput& input) {
    return visit_default(declarator, input);
}

VisitDeclaratorOutput Visitor::visit(Declarator* declarator, TypeDef* type_def, const VisitDeclaratorInput& input) {
    return visit_default(declarator, input);
}

VisitDeclaratorOutput Visitor::visit(Declarator* declarator, EnumConstant* enum_constant, const VisitDeclaratorInput& input) {
    return visit_default(declarator, input);
}


VisitTypeOutput Visitor::visit_default(const Type* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput Visitor::visit(const VoidType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

VisitTypeOutput Visitor::visit(const UniversalType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

VisitTypeOutput Visitor::visit(const IntegerType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

VisitTypeOutput Visitor::visit(const FloatingPointType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

VisitTypeOutput Visitor::visit(const PointerType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

VisitTypeOutput Visitor::visit(const QualifiedType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

VisitTypeOutput Visitor::visit(const UnqualifiedType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

VisitTypeOutput Visitor::visit(const FunctionType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

VisitTypeOutput Visitor::visit(const ResolvedArrayType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

VisitTypeOutput Visitor::visit(const StructType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

VisitTypeOutput Visitor::visit(const UnionType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

VisitTypeOutput Visitor::visit(const EnumType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

VisitTypeOutput Visitor::visit(const TypeOfType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

VisitTypeOutput Visitor::visit(const UnboundType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

VisitTypeOutput Visitor::visit(const UnresolvedArrayType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}

VisitTypeOutput Visitor::visit(const TypeDefType* type, const VisitTypeInput& input) {
    return visit_default(type, input);
}
