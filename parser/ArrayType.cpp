#include "ArrayType.h"
#include "Constant.h"
#include "Expr.h"
#include "Message.h"
#include "TranslationUnitContext.h"
#include "visitor/ResolvePass.h"
#include "visitor/Visitor.h"

ArrayType::ArrayType(const Type* element_type): element_type(element_type) {
}

UnresolvedArrayType::UnresolvedArrayType(const Type* element_type, Expr* size, const Location& location)
    : ArrayType(element_type), size(size), location(location) {
}

bool UnresolvedArrayType::is_complete() const {
    assert(false); // should be asked only of ResolvedArrayType
    return false;
}

VisitTypeOutput UnresolvedArrayType::accept(Visitor& visitor, const VisitTypeInput& input) const {
    return visitor.visit(this, input);
}

const Type* UnresolvedArrayType::resolve(ResolvePass& context) const {
    auto resolved_element_type = context.resolve(element_type);
    if (!resolved_element_type->is_complete()) {
        message(Severity::ERROR, location) << "incomplete array element type\n";
        resolved_element_type = IntegerType::default_type();
    }

    if (size) {
        size->resolve(context);
        auto size_constant = size->fold();
        unsigned long long size_int = 1;
        if (!size_constant.is_const_integer()) {
            message(Severity::ERROR, size->location) << "size of array must have integer type\n";
        } else {
            size_int = LLVMConstIntGetZExtValue(size_constant.value);
        }

        return ResolvedArrayType::of(ArrayKind::COMPLETE, resolved_element_type, size_int);
    } else {
        return ResolvedArrayType::of(ArrayKind::INCOMPLETE, resolved_element_type, 0);
    }
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

bool ResolvedArrayType::is_complete() const {
    return kind != ArrayKind::INCOMPLETE;
}

VisitTypeOutput ResolvedArrayType::accept(Visitor& visitor, const VisitTypeInput& input) const {
    return visitor.visit(this, input);
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
