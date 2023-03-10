#ifndef TYPE_CONTEXT_H
#define TYPE_CONTEXT_H

struct Type;
struct PointerType;
struct QualifiedType;
enum class TypeNameKind;

struct TypeContext {
    TypeContext() = default;
    TypeContext(const TypeContext&) = delete;
    ~TypeContext();

    void operator=(const TypeContext&) = delete;

    const Type* lookup_indexed_type(const string& key);
    void add_indexed_type(const string& key, const Type* type);

    const PointerType* lookup_pointer_type(const Type* base_type);
    void add_pointer_type(const PointerType* type);

    const QualifiedType* lookup_qualified_type(const Type* base_type, unsigned qualifiers);
    void add_qualified_type(const QualifiedType* type);

    // This map is only used for "complicated" types like functions.
    unordered_map<string, const Type*> indexed_types;

    typedef unordered_map<const Type*, const PointerType*> PointerTypesMap; 
    PointerTypesMap pointer_types;

    typedef map<pair<const Type*, unsigned>, const QualifiedType*> QualifierTypesMap; 
    QualifierTypesMap qualified_types;

};

#endif
