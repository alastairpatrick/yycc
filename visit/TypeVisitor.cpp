#include "TypeVisitor.h"

VisitTypeOutput TypeVisitor::visit(const VoidType* type) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const IntegerType* type) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const FloatingPointType* type) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const NestedType* type) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const PointerType* type) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const PassByReferenceType* type) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const QualifiedType* type) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const UnqualifiedType* type) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const FunctionType* type) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const ResolvedArrayType* type) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const StructType* type) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const UnionType* type) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const EnumType* type) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const TypeOfType* type) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const UnboundType* type) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const UnresolvedArrayType* type) {
    return VisitTypeOutput(type);
}

VisitTypeOutput TypeVisitor::visit(const TypeDefType* type) {
    return VisitTypeOutput(type);
}
