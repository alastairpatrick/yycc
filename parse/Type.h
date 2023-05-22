#ifndef PARSE_TYPE_H
#define PARSE_TYPE_H

#include "ASTNode.h"
#include "Identifier.h"
#include "lex/Location.h"
#include "lex/Token.h"
#include "Printable.h"
#include "Scope.h"
#include "Specifier.h"

struct Emitter;
struct Declaration;
struct EnumConstant;
struct Expr;
struct IdentifierMap;
enum class ScopeKind;
struct PointerType;
struct TypeDelegate;
struct TypeVisitor;
struct VisitTypeInput;

enum class TypePartition {
    FUNCTION,
    OBJECT,
    INCOMPLETE,
};

struct PrintType {
    explicit PrintType(const Type* type): type(type) {}
    const Type* type;
};

ostream& operator<<(ostream& stream, const PrintType& print_type);

struct Type: virtual Printable {
    Type() = default;
    Type(const Type&) = delete;
    void operator=(const Type&) = delete;

    virtual QualifierSet qualifiers() const;
    virtual const Type* unqualified() const;

    const PointerType* pointer_to() const;

    virtual TypePartition partition() const;  // C99 6.2.5p1
    virtual bool has_tag(const Declarator* declarator) const;

    virtual const Type* accept(TypeVisitor& visitor) const = 0;
    virtual LLVMTypeRef llvm_type() const;

    void message_print(ostream& stream) const;
    virtual void message_print(ostream& stream, int section) const = 0;
};

struct CachedType: Type {
    mutable LLVMTypeRef cached_llvm_type{};

    virtual LLVMTypeRef llvm_type() const override;
private:
    virtual LLVMTypeRef cache_llvm_type() const = 0;
};

struct VoidType: Type {
    static const VoidType it;

    virtual TypePartition partition() const override;
    virtual const Type* accept(TypeVisitor& visitor) const override;
    virtual LLVMTypeRef llvm_type() const override;
    virtual void message_print(ostream& stream, int section) const override;
    virtual void print(std::ostream& stream) const override;
};

enum class IntegerSize {
    BOOL,
    CHAR,
    SHORT,
    INT,
    LONG,
    LONG_LONG,
    NUM
};

enum class IntegerSignedness {
    DEFAULT, // Only valid for char
    SIGNED,
    UNSIGNED,
    NUM
};

struct IntegerType: Type {
    const IntegerSignedness signedness;
    const IntegerSize size;

    static const IntegerType* of_bool();
    static const IntegerType* of_char(bool is_wide);
    static const IntegerType* of_size(IntegerSignedness signedness);
    static const IntegerType* of(IntegerSignedness signedness, IntegerSize size);
    static const IntegerType* default_type();

    int num_bits() const;
    unsigned long long max() const;

    virtual const Type* accept(TypeVisitor& visitor) const override;
    virtual LLVMTypeRef llvm_type() const override;

    virtual void message_print(ostream& stream, int section) const override;
    virtual void print(std::ostream& stream) const override;
    
    bool is_signed() const;

private:
    IntegerType(IntegerSignedness signedness, IntegerSize size);
};

enum class FloatingPointSize {
    FLOAT,
    DOUBLE,
    LONG_DOUBLE,
    NUM
};

struct FloatingPointType: Type {
    const FloatingPointSize size;

    static const FloatingPointType* of(FloatingPointSize size);

    virtual const Type* accept(TypeVisitor& visitor) const override;
    virtual LLVMTypeRef llvm_type() const override;

    virtual void message_print(ostream& stream, int section) const override;
    virtual void print(std::ostream& stream) const override;

private:
    FloatingPointType(FloatingPointSize size);
};

struct PointerType: CachedType {
    const Type* const base_type;

    virtual const Type* accept(TypeVisitor& visitor) const override;
    virtual void message_print(ostream& stream, int section) const override;
    virtual void print(std::ostream& stream) const override;

private:
    friend class TypeContext;
    explicit PointerType(const Type* base_type);
    virtual LLVMTypeRef cache_llvm_type() const override;
};

struct PassByReferenceType: CachedType {
    enum class Kind {
        LVALUE,
        RVALUE,
    };

    const Type* const base_type;
    const Kind kind;

    static const PassByReferenceType* of(const Type* base_type, Kind kind);
    virtual const Type* accept(TypeVisitor& visitor) const override;
    virtual void message_print(ostream& stream, int section) const override;
    virtual void print(std::ostream& stream) const override;

private:
    friend class TypeContext;
    explicit PassByReferenceType(const Type* base_type, Kind kind);
    virtual LLVMTypeRef cache_llvm_type() const override;
};

struct QualifiedType: Type {
    const Type* const base_type;
    const QualifierSet qualifier_flags;

    static const Type* of(const Type* base_type, unsigned qualifiers);

    virtual QualifierSet qualifiers() const override;
    virtual const Type* unqualified() const override;
    virtual TypePartition partition() const override;

    virtual const Type* accept(TypeVisitor& visitor) const override;

    virtual LLVMTypeRef llvm_type() const override;

    virtual void message_print(ostream& stream, int section) const override;
    virtual void print(std::ostream& stream) const override;

private:
    friend class TypeContext;
    QualifiedType(const Type* base_type, unsigned qualifiers);
};

// Removes qualifiers on resolution. Used to implement typeof_unqual.
struct UnqualifiedType: ASTNode, Type {
    const Type* const base_type;

    explicit UnqualifiedType(const Type* base_type);
    virtual QualifierSet qualifiers() const override;
    virtual const Type* unqualified() const override;
    virtual const Type* accept(TypeVisitor& visitor) const override;
    virtual void message_print(ostream& stream, int section) const override;
    virtual void print(std::ostream& stream) const override;
};

struct FunctionType: CachedType {
    const Type* const return_type;
    const std::vector<const Type*> parameter_types;
    const bool variadic;

    static const FunctionType* of(const Type* return_type, vector<const Type*> parameter_types, bool variadic);

    virtual TypePartition partition() const override;
    virtual const Type* accept(TypeVisitor& visitor) const override;
    virtual void message_print(ostream& stream, int section) const override;
    virtual void print(std::ostream& stream) const override;
    
private:
    friend class TypeContext;
    FunctionType(const Type* return_type, std::vector<const Type*> parameter_types, bool variadic);
    virtual LLVMTypeRef cache_llvm_type() const override;
};

struct TagType: LocationNode, CachedType {
    mutable bool complete{};
    Declarator* tag{};
    Scope* scope{};

    TagType(const Location& location);
    virtual void message_print(ostream& stream, int section) const override;
};

// A StructuredType corresponds to an LLVM struct type. In C terms, it could be a struct type or a union type.
struct StructuredType: TagType {
    vector<Declaration*> declarations;
    mutable Declarator* destructor{};

    StructuredType(const Location& location);
    virtual TypePartition partition() const override;
    virtual bool has_tag(const Declarator* declarator) const override;
    virtual void print(std::ostream& stream) const override;

private:
    LLVMTypeRef build_llvm_struct_type(const vector<LLVMValueRef>& gep_indices_prefix, const char* name) const;
    virtual LLVMTypeRef cache_llvm_type() const override;
};

struct StructType: StructuredType {
    explicit StructType(const Location& location);
    virtual const Type* accept(TypeVisitor& visitor) const override;
    virtual void message_print(ostream& stream, int section) const override;
    virtual void print(std::ostream& stream) const override;
};

struct UnionType: StructuredType {
    explicit UnionType(const Location& location);
    virtual const Type* accept(TypeVisitor& visitor) const override;
    virtual void message_print(ostream& stream, int section) const override;
    virtual void print(std::ostream& stream) const override;
};

struct EnumType: TagType {
    mutable const Type* base_type{};
    mutable bool explicit_base_type{};
    vector<Declarator*> constants;

    explicit EnumType(const Location& location);
    virtual TypePartition partition() const override;
    virtual bool has_tag(const Declarator* declarator) const override;
    virtual const Type* accept(TypeVisitor& visitor) const override;
    virtual LLVMTypeRef cache_llvm_type() const override;
    virtual void message_print(ostream& stream, int section) const override;
    virtual void print(std::ostream& stream) const override;
};

struct TypeOfType: ASTNode, Type {
    const Location location;
    Expr* const expr;

    TypeOfType(Expr* expr, const Location& location);
    virtual TypePartition partition() const override;
    virtual const Type* accept(TypeVisitor& visitor) const override;
    virtual void message_print(ostream& stream, int section) const override;
    virtual void print(std::ostream& stream) const override;
};

// This type is only used during preparsing, when identifiers cannot necessarily be bound to declarators.
struct UnboundType: Type {
    const Identifier identifier;

    static const UnboundType* of(const Identifier& identifier);
    virtual const Type* accept(TypeVisitor& visitor) const override;
    virtual LLVMTypeRef llvm_type() const override;
    virtual void message_print(ostream& stream, int section) const override;
    virtual void print(ostream& stream) const override;

private:
    friend class TypeContext;
    explicit UnboundType(const Identifier& identifier);
};

struct TypeDefType: Type {
    Declarator* declarator;

    virtual const Type* unqualified() const override;
    virtual const Type* accept(TypeVisitor& visitor) const override;
    virtual LLVMTypeRef llvm_type() const override;
    virtual void message_print(ostream& stream, int section) const override;
    virtual void print(ostream& stream) const override;
};

struct NestedType: Type {
    const Type* enclosing_type;
    const Identifier identifier;
    const Location location;

    NestedType(const Type* enclosing_type, const Identifier& identifier, const Location& location);

    virtual const Type* accept(TypeVisitor& visitor) const override;
    virtual LLVMTypeRef llvm_type() const override;
    virtual void message_print(ostream& stream, int section) const override;
    virtual void print(ostream& stream) const override;
};

struct ThrowType: CachedType {
    const Type* base_type;

    static const ThrowType* of(const Type* base_type);
    virtual const Type* accept(TypeVisitor& visitor) const override;
    virtual void message_print(ostream& stream, int section) const override;
    virtual void print(std::ostream& stream) const override;

private:
    friend class TypeContext;
    explicit ThrowType(const Type* base_type);
    virtual LLVMTypeRef cache_llvm_type() const override;
};

#endif
