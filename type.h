#ifndef TYPE_H
#define TYPE_H

#include "llvm-c/Core.h"

#include "std.h"
#include "Identifier.h"
#include "Printable.h"
#include "Token.h"

struct CodeGenContext;
struct PointerType;
struct SymbolMap;

struct Type: Printable {
    virtual unsigned qualifiers() const;
    virtual const Type* unqualified() const;

    const PointerType* pointer_to() const;

    virtual const Type* promote() const;

    virtual const Type* resolve(SymbolMap& scope) const;

    virtual LLVMValueRef convert_to_type(CodeGenContext* context, LLVMValueRef value, const Type* to_type) const;

    virtual LLVMTypeRef llvm_type() const = 0;
};

struct VoidType: Type {
    static const VoidType it;
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
    FloatingPointType(FloatingPointSize size): size(size) {}
};

const Type* convert_arithmetic(const Type* left, const Type* right);

struct PointerType: Type {
    const Type* const base_type;

    virtual const Type* resolve(SymbolMap& scope) const;

    virtual LLVMTypeRef llvm_type() const;

    virtual void print(std::ostream& stream) const;

private:
    friend struct Type;

    mutable LLVMTypeRef llvm = nullptr;
    explicit PointerType(const Type* base_type);
};

enum TypeQualifiers {
    QUAL_CONST = 1 << TOK_CONST,
    QUAL_RESTRICT = 1 << TOK_RESTRICT,
    QUAL_VOLATILE = 1 << TOK_VOLATILE,
};

struct QualifiedType: Type {
    static const QualifiedType* of(const Type* base_type, unsigned qualifiers);

    const Type* const base_type;

    virtual unsigned qualifiers() const;
    virtual const Type* unqualified() const;

    virtual const Type* resolve(SymbolMap& scope) const;

    virtual LLVMTypeRef llvm_type() const;

    virtual void print(std::ostream& stream) const;

private:
    const unsigned qualifier_flags;
    explicit QualifiedType(const Type* base_type, unsigned qualifiers);
};

struct FunctionType: Type {
    static const FunctionType* of(const Type* return_type, std::vector<const Type*> parameter_types, bool variadic);

    const Type* return_type;
    const std::vector<const Type*> parameter_types;
    const bool variadic;

    virtual const Type* resolve(SymbolMap& scope) const;

    virtual LLVMTypeRef llvm_type() const;

    virtual void print(std::ostream& stream) const;
    
private:
    mutable LLVMTypeRef llvm = nullptr;
    FunctionType(const Type* return_type, std::vector<const Type*> parameter_types, bool variadic);
};

enum class TypeNameKind {
    ENUM,
    ORDINARY,
    STRUCT,
    UNION,
    NUM
};

#endif
