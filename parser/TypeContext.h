#ifndef PARSER_TYPE_CONTEXT_H
#define PARSER_TYPE_CONTEXT_H

#include "lexer/Identifier.h"
#include "lexer/Token.h"

enum class ArrayKind;
struct Type;
struct PointerType;
struct QualifiedType;
struct ResolvedArrayType;
struct UnboundType;

struct DerivedTypes {
    unique_ptr<const PointerType> pointer;
    unique_ptr<const ResolvedArrayType> incomplete_array;
    unique_ptr<const ResolvedArrayType> variable_length_array;
    unordered_map<unsigned long long, unique_ptr<const ResolvedArrayType>> complete_array;
    unordered_map<unsigned, unique_ptr<const QualifiedType>> qualified;
};

struct TypeContext {
    TypeContext();
    TypeContext(const TypeContext&) = delete;
    ~TypeContext();

    void operator=(const TypeContext&) = delete;

    const Type* lookup_indexed_type(const string& key);
    void add_indexed_type(const string& key, const Type* type);

    const PointerType* get_pointer_type(const Type* base_type);
    const ResolvedArrayType* get_array_type(ArrayKind kind, const Type* element_type, unsigned long long size);
    const QualifiedType* get_qualified_type(const Type* base_type, unsigned qualifiers);

    const UnboundType* get_unbound_type(const Identifier& identifier);

    // This map is only used for "complicated" types like functions.
    unordered_map<string, unique_ptr<const Type>> indexed_types;

    unordered_map<const Type*, DerivedTypes> derived_types;

    unordered_map<InternedString, unique_ptr<const UnboundType>> unbound_types;
};

#endif
