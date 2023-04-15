#ifndef PARSER_TYPE_H
#define PARSER_TYPE_H

#include "ASTNode.h"
#include "lexer/Identifier.h"
#include "lexer/Location.h"
#include "lexer/Token.h"
#include "Printable.h"

struct Emitter;
struct Declaration;
struct Declarator;
struct EnumConstant;
struct Expr;
struct IdentifierMap;
enum class IdentifierScope;
struct PointerType;
struct TypeDef;
struct Visitor;
struct VisitTypeInput;
struct VisitTypeOutput;

struct Type: virtual Printable {
    Type() = default;
    Type(const Type&) = delete;
    void operator=(const Type&) = delete;

    virtual unsigned qualifiers() const;
    virtual const Type* unqualified() const;

    const PointerType* pointer_to() const;

    virtual bool is_complete() const;
    virtual bool has_tag(const Declarator* declarator) const;

    virtual VisitTypeOutput accept(Visitor& visitor, const VisitTypeInput& input) const = 0;

    virtual const Type* compose(const Type* other) const;

    virtual const Type* promote() const;

    virtual const Type* compose_type_def_types(const Type* other) const;

    virtual LLVMValueRef convert_to_type(Emitter& context, LLVMValueRef value, const Type* to_type) const;

    virtual LLVMTypeRef llvm_type() const;
};

const Type* compose_types(const Type* a, const Type* b);
const Type* compose_type_def_types(const Type* a, const Type* b);

struct CachedType: Type {
    mutable LLVMTypeRef cached_llvm_type{};
    virtual LLVMTypeRef llvm_type() const override;

private:
    virtual LLVMTypeRef cache_llvm_type() const = 0;
};

struct VoidType: Type {
    static const VoidType it;
    virtual VisitTypeOutput accept(Visitor& visitor, const VisitTypeInput& input) const override;
    virtual LLVMTypeRef llvm_type() const override;
    virtual void print(std::ostream& stream) const override;
};

// UniversalType is compatible with all types.
struct UniversalType: Type {
    static const UniversalType it;
    virtual VisitTypeOutput accept(Visitor& visitor, const VisitTypeInput& input) const override;
    virtual LLVMTypeRef llvm_type() const override;
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
    static const IntegerType* of_char(bool is_wide);
    static const IntegerType* of(IntegerSignedness signedness, IntegerSize size);
    static const IntegerType* default_type();
    static const IntegerType* uintptr_type();

    const IntegerSignedness signedness;
    const IntegerSize size;

    virtual const Type* promote() const override;
    virtual LLVMValueRef convert_to_type(Emitter& context, LLVMValueRef value, const Type* to_type) const override;

    virtual VisitTypeOutput accept(Visitor& visitor, const VisitTypeInput& input) const override;
    virtual LLVMTypeRef llvm_type() const override;

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
    static const FloatingPointType* of(FloatingPointSize size);

    const FloatingPointSize size;

    virtual LLVMValueRef convert_to_type(Emitter& context, LLVMValueRef value, const Type* to_type) const override;

    virtual VisitTypeOutput accept(Visitor& visitor, const VisitTypeInput& input) const override;
    virtual LLVMTypeRef llvm_type() const override;

    virtual void print(std::ostream& stream) const override;

private:
    FloatingPointType(FloatingPointSize size);
};

const Type* convert_arithmetic(const Type* left, const Type* right);

struct PointerType: CachedType {
    const Type* const base_type;

    virtual VisitTypeOutput accept(Visitor& visitor, const VisitTypeInput& input) const override;
    virtual void print(std::ostream& stream) const override;

private:
    friend class TypeContext;
    explicit PointerType(const Type* base_type);
    virtual LLVMTypeRef cache_llvm_type() const override;
};

enum TypeQualifiers {
    QUAL_CONST = 1 << TOK_CONST,
    QUAL_RESTRICT = 1 << TOK_RESTRICT,
    QUAL_VOLATILE = 1 << TOK_VOLATILE,
};

struct QualifiedType: Type {
    static const Type* of(const Type* base_type, unsigned qualifiers);

    const Type* const base_type;
    const unsigned qualifier_flags;

    virtual unsigned qualifiers() const override;
    virtual const Type* unqualified() const override;
    virtual bool is_complete() const override;

    virtual VisitTypeOutput accept(Visitor& visitor, const VisitTypeInput& input) const override;

    virtual LLVMTypeRef llvm_type() const override;

    virtual void print(std::ostream& stream) const override;

private:
    friend class TypeContext;
    QualifiedType(const Type* base_type, unsigned qualifiers);
};

// Removes qualifiers on resolution. Used to implement typeof_unqual.
struct UnqualifiedType: ASTNode, Type {
    const Type* const base_type;

    explicit UnqualifiedType(const Type* base_type);
    virtual unsigned qualifiers() const override;
    virtual const Type* unqualified() const override;
    virtual VisitTypeOutput accept(Visitor& visitor, const VisitTypeInput& input) const override;
    virtual void print(std::ostream& stream) const override;
};

struct FunctionType: CachedType {
    static const FunctionType* of(const Type* return_type, vector<const Type*> parameter_types, bool variadic);

    const Type* const return_type;
    const std::vector<const Type*> parameter_types;
    const bool variadic;

    virtual bool is_complete() const override;
    virtual VisitTypeOutput accept(Visitor& visitor, const VisitTypeInput& input) const override;
    virtual void print(std::ostream& stream) const override;
    
private:
    friend class TypeContext;
    mutable LLVMTypeRef llvm = nullptr;
    FunctionType(const Type* return_type, std::vector<const Type*> parameter_types, bool variadic);
    virtual LLVMTypeRef cache_llvm_type() const override;
};

struct StructuredType: CachedType {
    StructuredType(const Location& location);

    const Location location;
    vector<Declarator*> members;
    unordered_map<InternedString, Declarator*> member_index;
    mutable bool complete{};
    Declarator* tag{};

    const Declarator* lookup_member(const Identifier& identifier) const;

    virtual bool is_complete() const override;
    virtual bool has_tag(const Declarator* declarator) const override;
    virtual void print(std::ostream& stream) const override;
};

struct StructType: StructuredType {
    explicit StructType(const Location& location);
    virtual VisitTypeOutput accept(Visitor& visitor, const VisitTypeInput& input) const override;
    virtual const Type* compose_type_def_types(const Type* other) const override;
    virtual void print(std::ostream& stream) const override;

private:
    virtual LLVMTypeRef cache_llvm_type() const override;
};

struct UnionType: StructuredType {
    explicit UnionType(const Location& location);
    virtual VisitTypeOutput accept(Visitor& visitor, const VisitTypeInput& input) const override;
    virtual const Type* compose_type_def_types(const Type* other) const override;
    virtual void print(std::ostream& stream) const override;

private:
    virtual LLVMTypeRef cache_llvm_type() const override;
};

struct EnumType: Type {
    explicit EnumType(const Location& location);
    
    const Location location;
    mutable const Type* base_type{};
    vector<EnumConstant*> constants;
    unordered_map<InternedString, EnumConstant*> constant_index;
    mutable bool complete{};
    Declarator* tag{};

    void add_constant(EnumConstant* constant);
    const EnumConstant* lookup_constant(const Identifier& identifier) const;

    virtual bool is_complete() const override;
    virtual bool has_tag(const Declarator* declarator) const override;
    virtual VisitTypeOutput accept(Visitor& visitor, const VisitTypeInput& input) const override;
    virtual LLVMTypeRef llvm_type() const override;
    virtual const Type* compose_type_def_types(const Type* other) const override;
    virtual void print(std::ostream& stream) const override;
};

struct TypeOfType: ASTNode, Type {
    const Location location;
    Expr* const expr;

    TypeOfType(Expr* expr, const Location& location);
    virtual bool is_complete() const override;
    virtual VisitTypeOutput accept(Visitor& visitor, const VisitTypeInput& input) const override;
    virtual void print(std::ostream& stream) const override;
};

// This type is only used during preparsing, when names cannot necessarily be bound to declarations.
struct UnboundType: Type {
    static const UnboundType* of(const Identifier& identifier);

    const Identifier identifier;

    virtual VisitTypeOutput accept(Visitor& visitor, const VisitTypeInput& input) const override;
    virtual LLVMTypeRef llvm_type() const override;
    virtual void print(ostream& stream) const override;

private:
    friend class TypeContext;
    explicit UnboundType(const Identifier& identifier);
};

struct TypeDefType: Type {
    TypeDefType(Declarator* declarator);

    Declarator* const declarator;

    virtual const Type* unqualified() const override;
    virtual VisitTypeOutput accept(Visitor& visitor, const VisitTypeInput& input) const override;
    virtual LLVMTypeRef llvm_type() const override;
    virtual void print(ostream& stream) const override;
};

#endif
