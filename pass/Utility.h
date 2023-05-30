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

#endif
