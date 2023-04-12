#ifndef PARSER_ARRAY_TYPE_H
#define PARSER_ARRAY_TYPE_H

#include "ASTNode.h"
#include "Type.h"

struct ArrayType: Type {
    const Type* const element_type;
protected:
    explicit ArrayType(const Type* element_type);
};

struct UnresolvedArrayType: ASTNode, ArrayType {
    const Expr* const size;

    UnresolvedArrayType(const Type* element_type, const Expr* size);
    virtual const Type* resolve(ResolutionContext& ctx) const;
    virtual void print(std::ostream& stream) const;
};

enum class ArrayKind {
    INCOMPLETE,
    COMPLETE,
    VARIABLE_LENGTH,
};

struct ResolvedArrayType: ArrayType {
    ArrayKind kind;
    unsigned long long size;

    static const ResolvedArrayType* of(ArrayKind kind, const Type* element_type, unsigned long long size);
    virtual const Type* compose(const Type* other) const;
    virtual void print(std::ostream& stream) const;

private:
    friend class TypeContext;
    ResolvedArrayType(ArrayKind kind, const Type* element_type, unsigned long long size);
};

#endif
