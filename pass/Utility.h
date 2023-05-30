#ifndef PASS_UTILITY_H
#define PASS_UTILITY_H

#include "parse/ArrayType.h"
#include "parse/Expr.h"

enum class ConvKind {
    IMPLICIT,
    C_IMPLICIT, // conversions that need not be explicit in C
    EXPLICIT,
};

template <typename T, typename U>
inline const T* unqualified_type_cast(const U* type) {
    assert(type->qualifiers() == 0);
    return dynamic_cast<const T*>(type);
}

ConvKind check_pointer_conversion(const Type* source_base_type, const Type* dest_base_type);
bool is_string_initializer(const ResolvedArrayType* array_type, const InitializerExpr* initializer);
bool values_are_aliases(LLVMValueRef a, LLVMValueRef b);

inline bool discards_qualifiers(QualifierSet from, QualifierSet to) {
    return (from | to) > to;
}

inline bool is_void_type(const Type* type) {
    return type->unqualified() == &VoidType::it;
}

#endif
