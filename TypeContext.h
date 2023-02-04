#ifndef TYPE_CONTEXT_H
#define TYPE_CONTEXT_H

#include "std.h"

struct Type;
struct QualifiedType;
struct TypeName;
enum class TypeNameKind;

struct TypeContext {
	const Type* lookup_indexed_type(const string& key);
	void add_indexed_type(const string& key, const Type* type);

	const QualifiedType* lookup_qualified_type(const Type* base_type, int qualifiers);
	void add_qualified_type(const QualifiedType* type);

	const TypeName* lookup_type_name(TypeNameKind kind, const string& name);
	void add_type_name(const TypeName* type);

	// This map is only used for "complicated" types like functions.
	unordered_map<string, const Type*> indexed_types;

	typedef map<pair<const Type*, int>, const QualifiedType*> QualifierTypesMap; 
	QualifierTypesMap qualified_types;

	struct TypeNameMapValue {
		const TypeName* kinds[4];
	};
	typedef unordered_map<string, TypeNameMapValue> TypeNameMap;
	TypeNameMap type_names;
};

#endif
