#include "TypeContext.h"
#include "ArrayType.h"

TypeContext::TypeContext() {
}

TypeContext::~TypeContext() {
}

const QualifiedType* TypeContext::get_qualified_type(const Type* base_type, unsigned qualifiers) {
    auto& derived = derived_types[base_type];
    auto& qualified = derived.qualified[qualifiers];
    if (!qualified) qualified.reset(new QualifiedType(base_type, qualifiers));
    return qualified.get();
}

const PointerType* TypeContext::get_pointer_type(const Type* base_type) {
    auto& derived = derived_types[base_type];
    if (!derived.pointer) derived.pointer.reset(new PointerType(base_type));
    return derived.pointer.get();
}

const ReferenceType* TypeContext::get_reference_type(const Type* base_type, ReferenceType::Kind kind) {
    auto& derived = derived_types[base_type];
    switch (kind) {
      case ReferenceType::Kind::LVALUE:
        if (!derived.lvalue_reference) derived.lvalue_reference.reset(new ReferenceType(base_type, kind));
        return derived.lvalue_reference.get();
      case ReferenceType::Kind::RVALUE:
        if (!derived.rvalue_reference) derived.rvalue_reference.reset(new ReferenceType(base_type, kind));
        return derived.rvalue_reference.get();
    }
}

const ThrowType* TypeContext::get_throw_type(const Type* base_type) {
    auto& derived = derived_types[base_type];
    if (!derived.throw_type) derived.throw_type.reset(new ThrowType(base_type));
    return derived.throw_type.get();
}

const ResolvedArrayType* TypeContext::get_array_type(ArrayKind kind, const Type* element_type, unsigned long long size) {
    auto& derived = derived_types[element_type];
    switch (kind) {
      case ArrayKind::INCOMPLETE:
        if (!derived.incomplete_array) derived.incomplete_array.reset(new ResolvedArrayType(kind, element_type, 0));
        return derived.incomplete_array.get();
      case ArrayKind::VARIABLE_LENGTH:
        if (!derived.variable_length_array) derived.variable_length_array.reset(new ResolvedArrayType(kind, element_type, 0));
        return derived.variable_length_array.get();
    }

    auto& complete = derived.complete_array[size];
    if (!complete) complete.reset(new ResolvedArrayType(kind, element_type, size));
    return complete.get();
}

const FunctionType* TypeContext::get_function_type(const Type* return_type, vector<const Type*> param_types, bool variadic) {
    auto& derived = derived_types[return_type];
    auto& function_map = variadic ? derived.function : derived.variadic_function;
    auto& function = function_map[param_types];
    if (!function) function.reset(new FunctionType(return_type, move(param_types), variadic));
    return function.get();
}

const UnboundType* TypeContext::get_unbound_type(const Identifier& identifier) {
    auto& unbound = unbound_types[identifier.text];
    if (!unbound) unbound.reset(new UnboundType(identifier));
    return unbound.get();
}
