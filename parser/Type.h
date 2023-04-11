#ifndef AST_TYPE_H
#define AST_TYPE_H

#include "lexer/Identifier.h"
#include "lexer/Location.h"
#include "lexer/Token.h"
#include "Printable.h"

struct CodeGenContext;
struct Declaration;
struct Declarator;
struct EnumConstant;
struct Expr;
enum class IdentifierScope;
struct PointerType;
struct ResolutionContext;
struct TypeDef;

struct Type: Printable {
    Type() = default;
    Type(const Type&) = delete;
    void operator=(const Type&) = delete;

    virtual unsigned qualifiers() const;
    virtual const Type* unqualified() const;

    const PointerType* pointer_to() const;

    virtual const Type* compose(const Type* other) const;

    virtual const Type* promote() const;

    virtual const Type* resolve(ResolutionContext& ctx) const;
    virtual const Type* compose_type_def_types(const Type* other) const;

    virtual LLVMValueRef convert_to_type(CodeGenContext* context, LLVMValueRef value, const Type* to_type) const;

    virtual LLVMTypeRef llvm_type() const = 0;
};

const Type* compose_types(const Type* a, const Type* b);
const Type* compose_type_def_types(const Type* a, const Type* b);

struct VoidType: Type {
    static const VoidType it;
    virtual LLVMTypeRef llvm_type() const;
    virtual void print(std::ostream& stream) const;
};

// CompatibleType is compatible with all types.
struct CompatibleType: Type {
    static const CompatibleType it;
    virtual LLVMTypeRef llvm_type() const;
    virtual void print(std::ostream& stream) const;
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

    const IntegerSignedness signedness;
    const IntegerSize size;

    virtual const Type* promote() const;
    virtual LLVMValueRef convert_to_type(CodeGenContext* context, LLVMValueRef value, const Type* to_type) const;

    virtual LLVMTypeRef llvm_type() const;

    virtual void print(std::ostream& stream) const;
    
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

    virtual LLVMValueRef convert_to_type(CodeGenContext* context, LLVMValueRef value, const Type* to_type) const;

    virtual LLVMTypeRef llvm_type() const;

    virtual void print(std::ostream& stream) const;

private:
    FloatingPointType(FloatingPointSize size);
};

const Type* convert_arithmetic(const Type* left, const Type* right);

struct PointerType: Type {
    const Type* const base_type;

    virtual const Type* resolve(ResolutionContext& ctx) const;

    virtual LLVMTypeRef llvm_type() const;

    virtual void print(std::ostream& stream) const;

private:
    friend struct Type;

    mutable LLVMTypeRef llvm = nullptr;
    explicit PointerType(const Type* base_type);
};

struct ArrayType: Type {
    ArrayType(const Type* element_type, const Expr* size);

    const Type* const element_type;
    const Expr* const size;

    virtual const Type* compose(const Type* other) const;

    virtual const Type* resolve(ResolutionContext& ctx) const;

    virtual LLVMTypeRef llvm_type() const;

    virtual void print(std::ostream& stream) const;
};

enum TypeQualifiers {
    QUAL_CONST = 1 << TOK_CONST,
    QUAL_RESTRICT = 1 << TOK_RESTRICT,
    QUAL_VOLATILE = 1 << TOK_VOLATILE,
};

struct QualifiedType: Type {
    static const Type* of(const Type* base_type, unsigned qualifiers);

    const Type* const base_type;

    virtual unsigned qualifiers() const;
    virtual const Type* unqualified() const;

    virtual const Type* resolve(ResolutionContext& ctx) const;

    virtual LLVMTypeRef llvm_type() const;

    virtual void print(std::ostream& stream) const;

private:
    const unsigned qualifier_flags;
    explicit QualifiedType(const Type* base_type, unsigned qualifiers);
};

struct FunctionType: Type {
    static const FunctionType* of(const Type* return_type, vector<const Type*> parameter_types, bool variadic);

    const Type* const return_type;
    const std::vector<const Type*> parameter_types;
    const bool variadic;

    virtual const Type* resolve(ResolutionContext& ctx) const;

    virtual LLVMTypeRef llvm_type() const;

    virtual void print(std::ostream& stream) const;
    
private:
    mutable LLVMTypeRef llvm = nullptr;
    FunctionType(const Type* return_type, std::vector<const Type*> parameter_types, bool variadic);
};

struct StructuredType: Type {
    StructuredType(const Location& location);

    const Location location;
    vector<Declarator*> members;
    bool complete{};

    virtual LLVMTypeRef llvm_type() const;

    virtual void print(std::ostream& stream) const;
};

struct StructType: StructuredType {
    explicit StructType(const Location& location);
    virtual const Type* compose_type_def_types(const Type* other) const;
    virtual void print(std::ostream& stream) const;
};

struct UnionType: StructuredType {
    explicit UnionType(const Location& location);
    virtual const Type* compose_type_def_types(const Type* other) const;
    virtual void print(std::ostream& stream) const;
};

struct EnumType: Type {
    explicit EnumType(const Location& location);
    
    const Location location;
    vector<EnumConstant*> constants;
    bool complete{};
    
    virtual LLVMTypeRef llvm_type() const;
    virtual void print(std::ostream& stream) const;
};

// This type is only used during preparsing, when names cannot necessarily be bound to declarations.
struct UnboundType: Type {
    static const UnboundType* of(const Identifier& identifier);

    const Identifier identifier;

    virtual LLVMTypeRef llvm_type() const;
    virtual void print(ostream& stream) const;

private:
    explicit UnboundType(const Identifier& identifier);
};

struct TypeDefType: Type {
    TypeDefType(Declarator* declarator);

    Declarator* const declarator;

    virtual const Type* unqualified() const;
    virtual const Type* resolve(ResolutionContext& ctx) const;
    virtual LLVMTypeRef llvm_type() const;
    virtual void print(ostream& stream) const;
};

#endif
