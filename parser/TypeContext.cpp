#include "TypeContext.h"

#include "Type.h"

TypeContext::TypeContext() {
}

TypeContext::~TypeContext() {
}

const Type* TypeContext::lookup_indexed_type(const string& key) {
    auto it = indexed_types.find(key);
    if (it == indexed_types.end()) return nullptr;
    return it->second.get();
}

void TypeContext::add_indexed_type(const string& key, const Type* type) {
    indexed_types[key].reset(type);
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

const UnboundType* TypeContext::get_unbound_type(const Identifier& identifier) {
    auto& unbound = unbound_types[identifier.name];
    if (!unbound) unbound.reset(new UnboundType(identifier));
    return unbound.get();
}
