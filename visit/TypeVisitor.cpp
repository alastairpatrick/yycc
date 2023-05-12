#include "TypeVisitor.h"

VisitTypeOutput TypeVisitor::visit(const VoidType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const IntegerType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const FloatingPointType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const NestedType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const PointerType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const PassByReferenceType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const QualifiedType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const UnqualifiedType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const FunctionType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const ResolvedArrayType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const StructType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const UnionType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const EnumType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const TypeOfType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const UnboundType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const UnresolvedArrayType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const TypeDefType* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type);
}
