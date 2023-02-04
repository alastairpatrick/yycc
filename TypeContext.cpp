#include "TypeContext.h"

#include "type.h"

const Type* TypeContext::lookup_indexed_type(const string& key) {
	auto it = indexed_types.find(key);
	if (it == indexed_types.end()) return nullptr;
	return it->second;
}

void TypeContext::add_indexed_type(const string& key, const Type* type) {
	indexed_types[key] = type;
}

const QualifiedType* TypeContext::lookup_qualified_type(const Type* base_type, int qualifiers) {
	TypeContext::QualifierTypesMap::key_type key(base_type, qualifiers);

	auto it = qualified_types.find(key);
	if (it == qualified_types.end()) return nullptr;	
	return it->second;
}

void TypeContext::add_qualified_type(const QualifiedType* type) {
	TypeContext::QualifierTypesMap::key_type key(type->base_type, type->qualifiers());
	qualified_types[key] = type;
}

const TypeName* TypeContext::lookup_type_name(TypeNameKind kind, const string& name) {
	auto it = type_names.find(name);
	if (it == type_names.end()) return nullptr;
	return it->second.kinds[unsigned(kind)];
}

void TypeContext::add_type_name(const TypeName* type) {
	type_names[type->name].kinds[unsigned(type->kind)] = type;
}
