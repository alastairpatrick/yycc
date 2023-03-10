#include "Type.h"
#include "CodeGenContext.h"
#include "CompileContext.h"
#include "Declaration.h"
#include "InternedString.h"
#include "SymbolMap.h"

// Type codes
// C char
// F float: Ff float, Fd double, Fl long double
// N name: Ns<id> struct, Nu<id> union, Ne<id> enum, Nt<id> typedef
// P pointer
// Q qualified: Qc const, Qv volatile, Qr restrict, Qcrv const restrict volatile, etc
// S signed: Sc signed char, Ss signed short, Si signed int, Sl signed long, Sm signed long long
// U unsigned: Ub bool, Uc unsigned char, Us unsigned short, Ui unsigned int, Ul unsigned long, Um unsigned long long
// V void
// . variadic placeholder

unsigned Type::qualifiers() const {
    return 0;
}

const Type* Type::unqualified() const {
    return this;
}

const Type* Type::promote() const {
    return this;
}

const Type* Type::resolve(SymbolMap& scope) const {
    return this;
}

LLVMValueRef Type::convert_to_type(CodeGenContext* context, LLVMValueRef value, const Type* to_type) const {
    if (to_type == this) return value;

    assert(false);  // TODO
    return value;
}

const PointerType* Type::pointer_to() const {
    auto type = CompileContext::it->type.lookup_pointer_type(this);
    if (type) return type;

    type = new PointerType(this);
    CompileContext::it->type.add_pointer_type(type);

    return type;
}

const VoidType VoidType::it;

LLVMTypeRef VoidType::llvm_type() const {
    return LLVMVoidType();
}

void VoidType::print(std::ostream& stream) const {
    stream << "V";
}

const IntegerType* IntegerType::of_char(bool is_wide) {
    // As with C and C++, and for the same reasons, there are three distinct character
    // types: char, signed char and unsigned char.
    // 
    // The size and signedness of wide characters depends on how wchar_t is typedef-ed
    // by the C standard library. The compiler must be consistent.
    return is_wide ? IntegerType::of(IntegerSignedness::UNSIGNED, IntegerSize::SHORT)
                   : IntegerType::of(IntegerSignedness::DEFAULT, IntegerSize::CHAR);
}

const IntegerType* IntegerType::of(IntegerSignedness signedness, IntegerSize size) {
    static const IntegerType types[int(IntegerSignedness::NUM)][int(IntegerSize::NUM)] = {
        {
            IntegerType(IntegerSignedness::DEFAULT, IntegerSize::BOOL),
            IntegerType(IntegerSignedness::DEFAULT, IntegerSize::CHAR),
            IntegerType(IntegerSignedness::DEFAULT, IntegerSize::SHORT),
            IntegerType(IntegerSignedness::DEFAULT, IntegerSize::INT),
            IntegerType(IntegerSignedness::DEFAULT, IntegerSize::LONG),
            IntegerType(IntegerSignedness::DEFAULT, IntegerSize::LONG_LONG),
        },
        {
            IntegerType(IntegerSignedness::SIGNED, IntegerSize::BOOL),
            IntegerType(IntegerSignedness::SIGNED, IntegerSize::CHAR),
            IntegerType(IntegerSignedness::SIGNED, IntegerSize::SHORT),
            IntegerType(IntegerSignedness::SIGNED, IntegerSize::INT),
            IntegerType(IntegerSignedness::SIGNED, IntegerSize::LONG),
            IntegerType(IntegerSignedness::SIGNED, IntegerSize::LONG_LONG),
        },
        {
            IntegerType(IntegerSignedness::UNSIGNED, IntegerSize::BOOL),
            IntegerType(IntegerSignedness::UNSIGNED, IntegerSize::CHAR),
            IntegerType(IntegerSignedness::UNSIGNED, IntegerSize::SHORT),
            IntegerType(IntegerSignedness::UNSIGNED, IntegerSize::INT),
            IntegerType(IntegerSignedness::UNSIGNED, IntegerSize::LONG),
            IntegerType(IntegerSignedness::UNSIGNED, IntegerSize::LONG_LONG),
        },
    };

    assert(size < IntegerSize::NUM);
    return &types[int(signedness)][int(size)];
}

const IntegerType* IntegerType::default_type() {
    return of(IntegerSignedness::SIGNED, IntegerSize::INT);
}

const Type* IntegerType::promote() const {
    auto int_type = IntegerType::default_type();

    // Integer types smaller than int are promoted when an operation is performed on them.
    if (size < int_type->size || (signedness == IntegerSignedness::SIGNED && size == int_type->size)) {
        // If all values of the original type can be represented as an int, the value of the smaller type is converted to an int; otherwise, it is converted to an unsigned int.
        return int_type;
    }
    
    return this;
}

LLVMValueRef IntegerType::convert_to_type(CodeGenContext* context, LLVMValueRef value, const Type* to_type) const {
    auto builder = context->builder;

    if (to_type == this) return value;

    if (auto to_as_int = dynamic_cast<const IntegerType*>(to_type)) {
        if (to_as_int->size == size) return value;

        if (to_as_int->size > size) {
            if (is_signed()) {
                return LLVMBuildSExt(builder, value, to_type->llvm_type(), "cvt");
            } else {
                return LLVMBuildZExt(builder, value, to_type->llvm_type(), "cvt");
            }
        }

        return LLVMBuildTrunc(builder, value, to_type->llvm_type(), 0);
    }

    if (auto to_as_float = dynamic_cast<const FloatingPointType*>(to_type)) {
        if (is_signed()) {
            return LLVMBuildSIToFP(builder, value, to_type->llvm_type(), "cvt");
        } else {
            return LLVMBuildUIToFP(builder, value, to_type->llvm_type(), "cvt");
        }
    }

    assert(false); // TODO
    return nullptr;
}

LLVMTypeRef IntegerType::llvm_type() const {
    static const LLVMTypeRef types[unsigned(IntegerSize::NUM)] = {
        LLVMInt1Type(), 
        LLVMInt8Type(), 
        LLVMInt16Type(),
        LLVMInt32Type(),
        LLVMInt32Type(),
        LLVMInt64Type(),
    };
    return types[unsigned(size)];
}

void IntegerType::print(ostream& stream) const {
    if (signedness == IntegerSignedness::DEFAULT) {
        stream << 'C';
    } else {
        static const char* const signednesses[unsigned(IntegerSignedness::NUM)] = { nullptr, "S", "U" };
        static const char* const sizes[unsigned(IntegerSize::NUM)] = { "b", "c", "s", "i", "l", "m" };
        stream << signednesses[unsigned(signedness)] << sizes[unsigned(size)];
    }
}

bool IntegerType::is_signed() const {
    if (signedness == IntegerSignedness::DEFAULT) {
        assert(size == IntegerSize::CHAR);

        // This should be platform dependent.
        return true;
    }

    return signedness == IntegerSignedness::SIGNED;
}

IntegerType::IntegerType(IntegerSignedness signedness, IntegerSize size)
    : signedness(signedness), size(size) {}

const FloatingPointType* FloatingPointType::of(FloatingPointSize size) {
    static const FloatingPointType types[int(FloatingPointSize::NUM)] = {
        FloatingPointType(FloatingPointSize::FLOAT),
        FloatingPointType(FloatingPointSize::DOUBLE),
        FloatingPointType(FloatingPointSize::LONG_DOUBLE),
    };

    assert(size < FloatingPointSize::NUM);
    return &types[int(size)];
}


LLVMValueRef FloatingPointType::convert_to_type(CodeGenContext* context, LLVMValueRef value, const Type* to_type) const {
    auto builder = context->builder;

    if (to_type == this) return value;

    if (auto to_as_float = dynamic_cast<const FloatingPointType*>(to_type)) {
        if (to_as_float->size > size) {
            return LLVMBuildFPExt(builder, value, to_type->llvm_type(), "cvt");
        } else {
            return LLVMBuildFPTrunc(builder, value, to_type->llvm_type(), "cvt");
        }
    }

    if (auto to_as_int = dynamic_cast<const IntegerType*>(to_type)) {
        if (to_as_int->is_signed()) {
            return LLVMBuildFPToSI(builder, value, to_type->llvm_type(), "cvt");
        } else {
            return LLVMBuildFPToUI(builder, value, to_type->llvm_type(), "cvt");
        }
    }

    assert(false);  // TODO
    return value;
}

LLVMTypeRef FloatingPointType::llvm_type() const {
    static const LLVMTypeRef types[unsigned(FloatingPointSize::NUM)] = {
        LLVMFloatType(), 
        LLVMDoubleType(),
        LLVMX86FP80Type(),
    };
    return types[unsigned(size)];
}

void FloatingPointType::print(ostream& stream) const {
    static const char* const types[int(IntegerSize::NUM)] = { "f", "d", "l" };
    stream << 'F' << types[unsigned(size)];
}

// This is applied to binary expressions and to the second and third operands of a conditional expression.
const Type* convert_arithmetic(const Type* left, const Type* right) {
    left = left->promote();
    right = right->promote();

    // If both operands have the same type, no further conversion is needed.
    if (left == right) return left;

    auto left_as_float = dynamic_cast<const FloatingPointType*>(left);
    auto right_as_float = dynamic_cast<const FloatingPointType*>(right);
    if (left_as_float) {
        if (right_as_float) {
            return FloatingPointType::of(max(left_as_float->size, right_as_float->size));
        }
        return left_as_float;
    }
    if (right_as_float) return right_as_float;

    auto left_as_int = dynamic_cast<const IntegerType*>(left);
    auto right_as_int = dynamic_cast<const IntegerType*>(right);

    if (left_as_int && right_as_int) {
        // char was promoted to signed int so DEFAULT is impossible.
        assert(left_as_int ->signedness != IntegerSignedness::DEFAULT);
        assert(right_as_int->signedness != IntegerSignedness::DEFAULT);

        // If both operands are of the same integer type (signed or unsigned), the operand with the type of lesser integer conversion rank is converted to the type of the operand with greater rank.
        if (left_as_int->signedness == right_as_int->signedness) {
            return IntegerType::of(left_as_int->signedness, max(left_as_int->size, right_as_int->size));
        }

        // If the operand that has unsigned integer type has rank greater than or equal to the rank of the type of the other operand, the operand with signed integer type is converted to the type of the operand with unsigned integer type.
        auto unsigned_int = left_as_int->signedness == IntegerSignedness::UNSIGNED ? left_as_int : right_as_int;
        auto signed_int = left_as_int->signedness == IntegerSignedness::SIGNED ? left_as_int : right_as_int;
        if (unsigned_int->size >= signed_int->size) {
            return unsigned_int;
        }

        // If the type of the operand with signed integer type can represent all of the values of the type of the operand with unsigned integer type, the operand with unsigned integer type is converted to the type of the operand with signed integer type.
        if (signed_int->size > unsigned_int->size) {
            return signed_int;
        }

        // Otherwise, both operands are converted to the unsigned integer type corresponding to the type of the operand with signed integer type.
        return IntegerType::of(IntegerSignedness::UNSIGNED, signed_int->size);
    }

    // For class pointer
    assert(false); // TODO
    return nullptr;
}

const Type* PointerType::resolve(SymbolMap& scope) const {
    return base_type->resolve(scope)->pointer_to();
}

LLVMTypeRef PointerType::llvm_type() const {
    if (!llvm) llvm = LLVMPointerType(base_type->llvm_type(), 0);
    return llvm;
}

void PointerType::print(std::ostream& stream) const {
    stream << 'P' << base_type;
}

PointerType::PointerType(const Type* base_type)
    : base_type(base_type) {
}

const QualifiedType* QualifiedType::of(const Type* base_type, unsigned qualifiers) {
    qualifiers |= base_type->qualifiers();
    base_type = base_type->unqualified();

    auto type = CompileContext::it->type.lookup_qualified_type(base_type, qualifiers);
    if (type) return type;

    type = new QualifiedType(base_type, qualifiers);
    CompileContext::it->type.add_qualified_type(type);
    return type;
}

unsigned QualifiedType::qualifiers() const {
    return qualifier_flags;
}

const Type* QualifiedType::unqualified() const {
    return base_type;
}

const Type* QualifiedType::resolve(SymbolMap& scope) const {
    return QualifiedType::of(base_type->resolve(scope), qualifier_flags);
}

LLVMTypeRef QualifiedType::llvm_type() const {
    return base_type->llvm_type();
}

void QualifiedType::print(std::ostream& stream) const {
    stream << 'Q';
    if (qualifier_flags & QUAL_CONST) stream << 'c';
    if (qualifier_flags & QUAL_RESTRICT) stream << 'r';
    if (qualifier_flags & QUAL_VOLATILE) stream << 'v';
    stream << base_type;
}

QualifiedType::QualifiedType(const Type* base_type, unsigned qualifiers)
    : base_type(base_type), qualifier_flags(qualifiers) {
}

static void print_function_type(ostream& stream, const Type* return_type, const std::vector<const Type*>& parameter_types, bool variadic) {
    stream << '(';

    for (auto type: parameter_types) stream << type;

    if (variadic) stream << '?';
    stream << ')' << return_type;
}

const FunctionType* FunctionType::of(const Type* return_type, std::vector<const Type*> parameter_types, bool variadic) {
    stringstream key_stream;
    print_function_type(key_stream, return_type, parameter_types, variadic);

    auto type = static_cast<const FunctionType*>(CompileContext::it->type.lookup_indexed_type(key_stream.str()));
    if (type) return type;

    type = new FunctionType(return_type, move(parameter_types), variadic);
    CompileContext::it->type.add_indexed_type(key_stream.str(), type);
    return type;
}

const Type* FunctionType::resolve(SymbolMap& scope) const {
    auto resolved_return_type = return_type->resolve(scope);
    auto resolved_param_types(parameter_types);
    for (auto& param_type : resolved_param_types) {
        param_type = param_type->resolve(scope);
    }
    return FunctionType::of(resolved_return_type, resolved_param_types, variadic);
}

LLVMTypeRef FunctionType::llvm_type() const {
    if (llvm) return llvm;

    vector<LLVMTypeRef> param_llvm;
    param_llvm.reserve(parameter_types.size());

    for (auto type: parameter_types) param_llvm.push_back(type->llvm_type());

    llvm = LLVMFunctionType(return_type->llvm_type(), param_llvm.data(), unsigned int(param_llvm.size()), variadic);

    return llvm;
}

void FunctionType::print(std::ostream& stream) const {
    print_function_type(stream, return_type, parameter_types, variadic);
}

FunctionType::FunctionType(const Type* return_type, std::vector<const Type*> parameter_types, bool variadic)
    : return_type(return_type), parameter_types(move(parameter_types)), variadic(variadic) {
}

const NamedType* NamedType::of(TypeNameKind kind, const Identifier& identifier) {
    auto type = CompileContext::it->type.lookup_named_type(kind, identifier);
    if (type) return type;

    type = new NamedType(kind, identifier);
    CompileContext::it->type.add_named_type(kind, identifier, type);
    return type;
}

LLVMTypeRef NamedType::llvm_type() const {
    assert(false);
    return nullptr;
}

void NamedType::print(ostream& stream) const {
    stream << 'N';

    switch (kind) {
    case TypeNameKind::ENUM:
        stream << 'e';
        break;
    case TypeNameKind::ORDINARY:
        stream << 't';
        break;
    case TypeNameKind::STRUCT:
        stream << 's';
        break;
    case TypeNameKind::UNION:
        stream << 'u';
        break;
    }

    stream << *identifier.name;
}

NamedType::NamedType(TypeNameKind kind, const Identifier& identifier)
    : kind(kind), identifier(identifier) {
}
