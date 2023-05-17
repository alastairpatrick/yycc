#ifndef VISIT_TYPE_VISITOR_H
#define VISIT_TYPE_VISITOR_H

#include "parse/ArrayType.h"
#include "parse/Type.h"

struct TypeVisitor {
    virtual const Type* visit(const EnumType* type) { return type; }
    virtual const Type* visit(const FloatingPointType* type) { return type; }
    virtual const Type* visit(const FunctionType* type) { return type; }
    virtual const Type* visit(const IntegerType* type) { return type; }
    virtual const Type* visit(const NestedType* type) { return type; }
    virtual const Type* visit(const PointerType* type) { return type; }
    virtual const Type* visit(const PassByReferenceType* type) { return type; }
    virtual const Type* visit(const QualifiedType* type) { return type; }
    virtual const Type* visit(const ResolvedArrayType* type) { return type; }
    virtual const Type* visit(const StructType* type) { return type; }
    virtual const Type* visit(const TypeDefType* type) { return type; }
    virtual const Type* visit(const TypeOfType* type) { return type; }
    virtual const Type* visit(const VoidType* type) { return type; }
    virtual const Type* visit(const UnboundType* type) { return type; }
    virtual const Type* visit(const UnionType* type) { return type; }
    virtual const Type* visit(const UnqualifiedType* type) { return type; }
    virtual const Type* visit(const UnresolvedArrayType* type) { return type; }
};

#endif

