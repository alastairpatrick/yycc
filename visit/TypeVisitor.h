#ifndef VISIT_TYPE_VISITOR_H
#define VISIT_TYPE_VISITOR_H

#include "parse/ArrayType.h"
#include "parse/Constant.h"
#include "parse/Expr.h"
#include "parse/Statement.h"
#include "parse/Type.h"
#include "Value.h"

enum class ConvKind {
    IMPLICIT,
    C_IMPLICIT, // conversions that need not be explicit in C
    EXPLICIT,
};

struct VisitTypeOutput {
    Value value;
    ConvKind conv_kind = ConvKind::IMPLICIT;

    VisitTypeOutput() = default;
    explicit VisitTypeOutput(Value value, ConvKind kind = ConvKind::IMPLICIT): value(value), conv_kind(kind) {}
    explicit VisitTypeOutput(const Type* type, LLVMValueRef value = nullptr, ConvKind kind = ConvKind::IMPLICIT): value(type, value), conv_kind(kind) {}
};

struct TypeVisitor {
    virtual VisitTypeOutput visit(const EnumType* type);
    virtual VisitTypeOutput visit(const FloatingPointType* type);
    virtual VisitTypeOutput visit(const FunctionType* type);
    virtual VisitTypeOutput visit(const IntegerType* type);
    virtual VisitTypeOutput visit(const NestedType* type);
    virtual VisitTypeOutput visit(const PointerType* type);
    virtual VisitTypeOutput visit(const PassByReferenceType* type);
    virtual VisitTypeOutput visit(const QualifiedType* type);
    virtual VisitTypeOutput visit(const ResolvedArrayType* type);
    virtual VisitTypeOutput visit(const StructType* type);
    virtual VisitTypeOutput visit(const TypeDefType* type);
    virtual VisitTypeOutput visit(const TypeOfType* type);
    virtual VisitTypeOutput visit(const VoidType* type);
    virtual VisitTypeOutput visit(const UnboundType* type);
    virtual VisitTypeOutput visit(const UnionType* type);
    virtual VisitTypeOutput visit(const UnqualifiedType* type);
    virtual VisitTypeOutput visit(const UnresolvedArrayType* type);
};

#endif

