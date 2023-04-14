#ifndef PARSER_ARRAY_TYPE_H
#define PARSER_ARRAY_TYPE_H

#include "ASTNode.h"
#include "Type.h"

struct ArrayType: CachedType {
    const Type* const element_type;
protected:
    explicit ArrayType(const Type* element_type);
};

struct UnresolvedArrayType: ASTNode, ArrayType {
    Location location;
    Expr* size;

    UnresolvedArrayType(const Type* element_type, Expr* size, const Location& location);
    virtual bool is_complete() const override;
    virtual const Type* resolve(ResolveContext& context) const override;
    virtual LLVMTypeRef cache_llvm_type() const override;
    virtual void print(std::ostream& stream) const override;
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
    virtual bool is_complete() const override;
    virtual const Type* compose(const Type* other) const override;
    virtual LLVMTypeRef cache_llvm_type() const override;
    virtual void print(std::ostream& stream) const override;

private:
    friend class TypeContext;
    ResolvedArrayType(ArrayKind kind, const Type* element_type, unsigned long long size);
};

#endif
