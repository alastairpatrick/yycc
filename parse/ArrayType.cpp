#include "ArrayType.h"
#include "Constant.h"
#include "Expr.h"
#include "Message.h"
#include "TranslationUnitContext.h"
#include "visit/Visitor.h"

ArrayType::ArrayType(const Type* element_type): element_type(element_type) {
}

UnresolvedArrayType::UnresolvedArrayType(const Type* element_type, Expr* size, const Location& location)
    : ArrayType(element_type), size(size), location(location) {
}

TypePartition UnresolvedArrayType::partition() const {
    assert(false); // should be asked only of ResolvedArrayType
    return TypePartition::INCOMPLETE;
}

VisitTypeOutput UnresolvedArrayType::accept(Visitor& visitor, const VisitTypeInput& input) const {
    return visitor.visit(this, input);
}

LLVMTypeRef UnresolvedArrayType::cache_llvm_type() const {
    assert(false);
    return nullptr;
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

TypePartition ResolvedArrayType::partition() const {
    return kind == ArrayKind::INCOMPLETE ? TypePartition::INCOMPLETE : TypePartition::OBJECT;
}

VisitTypeOutput ResolvedArrayType::accept(Visitor& visitor, const VisitTypeInput& input) const {
    return visitor.visit(this, input);
}

LLVMTypeRef ResolvedArrayType::cache_llvm_type() const {
    // TODO: use LLVMArrayType2 instead after upgrading LLVM
    return LLVMArrayType(element_type->llvm_type(), size);
}

void ResolvedArrayType::print(std::ostream& stream) const {
    stream << "[\"A\", " << element_type;
    if (size) {
        stream << ", " << size;
    }
    stream << ']';
}
