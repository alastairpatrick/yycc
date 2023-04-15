#ifndef PARSER_TYPE_CONTEXT_H
#define PARSER_TYPE_CONTEXT_H

#include "lexer/Identifier.h"
#include "lexer/Token.h"

enum class ArrayKind;
struct FunctionType;
struct Type;
struct PointerType;
struct QualifiedType;
struct ResolvedArrayType;
struct UnboundType;

template <typename T>
struct VectorHash {
    size_t operator()(const vector<T>& vec) const {
        std::hash<T> hash;
        size_t seed = 0;
        for (auto e: vec) {
            seed ^= hash(e) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};

struct DerivedTypes {
    unique_ptr<const PointerType> pointer;

    unordered_map<unsigned, unique_ptr<const QualifiedType>> qualified;

    unique_ptr<const ResolvedArrayType> incomplete_array;
    unique_ptr<const ResolvedArrayType> variable_length_array;
    unordered_map<unsigned long long, unique_ptr<const ResolvedArrayType>> complete_array;

    typedef unordered_map<vector<const Type*>, unique_ptr<const FunctionType>, VectorHash<const Type*>> FunctionMap;
    FunctionMap function;
    FunctionMap variadic_function;
};

struct TypeContext {
    TypeContext();
    TypeContext(const TypeContext&) = delete;
    ~TypeContext();

    void operator=(const TypeContext&) = delete;

    const PointerType* get_pointer_type(const Type* base_type);
    const QualifiedType* get_qualified_type(const Type* base_type, unsigned qualifiers);

    const ResolvedArrayType* get_array_type(ArrayKind kind, const Type* element_type, unsigned long long size);

    const FunctionType* get_function_type(const Type* return_type, vector<const Type*> param_types, bool variadic);

    const UnboundType* get_unbound_type(const Identifier& identifier);

    unordered_map<const Type*, DerivedTypes> derived_types;

    unordered_map<InternedString, unique_ptr<const UnboundType>> unbound_types;
};

#endif
