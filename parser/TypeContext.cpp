#include "TypeContext.h"

#include "Type.h"

TypeContext::~TypeContext() {
    // Could use unique_ptr but it actually turns into a giant mess and this is much simpler.

    for (auto& value : indexed_types) {
        delete value.second;
    }

    for (auto& value : pointer_types) {
        delete value.second;
    }

    for (auto& value : qualified_types) {
        delete value.second;
    }
}

const Type* TypeContext::lookup_indexed_type(const string& key) {
    auto it = indexed_types.find(key);
    if (it == indexed_types.end()) return nullptr;
    return it->second;
}

void TypeContext::add_indexed_type(const string& key, const Type* type) {
    indexed_types[key] = type;
}

const QualifiedType* TypeContext::lookup_qualified_type(const Type* base_type, unsigned qualifiers) {
    TypeContext::QualifierTypesMap::key_type key(base_type, qualifiers);

    auto it = qualified_types.find(key);
    if (it == qualified_types.end()) return nullptr;	
    return it->second;
}

void TypeContext::add_qualified_type(const QualifiedType* type) {
    TypeContext::QualifierTypesMap::key_type key(type->base_type, type->qualifiers());
    qualified_types[key] = type;
}

const PointerType* TypeContext::lookup_pointer_type(const Type* base_type) {
    auto it = pointer_types.find(base_type);
    if (it == pointer_types.end()) return nullptr;	
    return it->second;
}

void TypeContext::add_pointer_type(const PointerType* type) {
    pointer_types[type->base_type] = type;
}

const UnboundType* TypeContext::lookup_unbound_type(const Identifier& identifier) {
    auto it = unbound_types.find(identifier.name);
    if (it == unbound_types.end()) return nullptr;
    return it->second;
}

void TypeContext::add_unbound_type(const Identifier& identifier, const UnboundType* type) {
    unbound_types[identifier.name] = type;
}
