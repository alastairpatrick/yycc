#include "ArrayType.h"
#include "Constant.h"
#include "Expr.h"
#include "TranslationUnitContext.h"

ArrayType::ArrayType(const Type* element_type): element_type(element_type) {
}

UnresolvedArrayType::UnresolvedArrayType(const Type* element_type, const Expr* size)
    : ArrayType(element_type), size(size) {
}

const Type* UnresolvedArrayType::resolve(ResolutionContext& ctx) const {
    auto resolved_element_type = element_type->resolve(ctx);

    // TODO: properly evaluate constant size expression and return canonical representation of array type.
    auto size_constant = dynamic_cast<const IntegerConstant*>(size);

    return ResolvedArrayType::of(size_constant ? ArrayKind::COMPLETE : ArrayKind::INCOMPLETE,
                                 resolved_element_type,
                                 size_constant ? size_constant->uint_value() : 0);
}

void UnresolvedArrayType::print(std::ostream& stream) const {
    stream << "[\"A\", " << element_type;
    if (size) {
        stream << ", " << size;
    }
    stream << ']';
}

const ResolvedArrayType* ResolvedArrayType::of(ArrayKind kind, const Type* element_type, unsigned long long size) {
    return TranslationUnitContext::it->type.get_array_type(kind, element_type, size);
}

ResolvedArrayType::ResolvedArrayType(ArrayKind kind, const Type* element_type, unsigned long long size)
    : ArrayType(element_type), kind(kind), size(size) {
}

const Type* ResolvedArrayType::compose(const Type* o) const {
    auto other = static_cast<const ResolvedArrayType*>(o);

    if (element_type != other->element_type) return nullptr;

    if (kind == ArrayKind::INCOMPLETE) return other;
    if (other->kind == ArrayKind::INCOMPLETE) return this;

    if (size == other->size) {
        assert(other == this);
        return this;
    }

    return nullptr;
}

void ResolvedArrayType::print(std::ostream& stream) const {
    stream << "[\"A\", " << element_type;
    if (size) {
        stream << ", " << size;
    }
    stream << ']';
}
