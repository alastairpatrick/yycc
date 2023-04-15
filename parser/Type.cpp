#include "Type.h"

#include "ArrayType.h"
#include "Constant.h"
#include "Declaration.h"
#include "Expr.h"
#include "IdentifierMap.h"
#include "InternedString.h"
#include "Message.h"
#include "TranslationUnitContext.h"
#include "visitor/Visitor.h"

// Type codes
// A array
// C char
// D declaration
// F float: Ff float, Fd double, Fl long double
// N name: Ns<id> struct, Nu<id> union, Ne<id> enum, Nt<id> typedef
// P pointer
// Q qualified: Qc const, Qv volatile, Qr restrict, Qcrv const restrict volatile, etc
// R struct
// S signed: Sc signed char, Ss signed short, Si signed int, Sl signed long, Sm signed long long
// U unsigned: Ub bool, Uc unsigned char, Us unsigned short, Ui unsigned int, Ul unsigned long, Um unsigned long long
// V void
// . variadic placeholder

#pragma region Type

unsigned Type::qualifiers() const {
    return 0;
}

const Type* Type::unqualified() const {
    return this;
}

const Type* Type::promote() const {
    return this;
}

const Type* Type::compose_type_def_types(const Type* other) const {
    if (this == other) return this;
    return nullptr;
}

LLVMTypeRef Type::llvm_type() const {
    assert(false);
    return nullptr;
}

const Type* compose_types(const Type* a, const Type* b) {
    if (a == b) return a;

    if (a == &UniversalType::it) return b;
    if (b == &UniversalType::it) return a;

    if (typeid(*a) == typeid(*b)) {
        return a->compose(b);
    }

    return nullptr;
}

const Type* compose_type_def_types(const Type* a, const Type* b) {
    if (a == b) return a;

    if (typeid(*a) == typeid(*b)) {
        return a->compose_type_def_types(b);
    }

    return nullptr;
}

const Type* Type::compose(const Type* other) const {
    return nullptr;
}

LLVMTypeRef CachedType::llvm_type() const {
    if (cached_llvm_type) return cached_llvm_type;

    assert(is_complete());
    return cached_llvm_type = cache_llvm_type();
}

const PointerType* Type::pointer_to() const {
    return TranslationUnitContext::it->type.get_pointer_type(this);
}

bool Type::is_complete() const {
    return true;
}

bool Type::has_tag(const Declarator* declarator) const {
    return false;
}

#pragma endregion Type

#pragma region VoidType

const VoidType VoidType::it;

VisitTypeOutput VoidType::accept(Visitor& visitor, const VisitTypeInput& input) const {
    return visitor.visit(this, input);
}

LLVMTypeRef VoidType::llvm_type() const {
    return LLVMVoidType();
}

void VoidType::print(std::ostream& stream) const {
    stream << "\"V\"";
}

#pragma endregion VoidType

#pragma region UniversalType

const UniversalType UniversalType::it;

VisitTypeOutput UniversalType::accept(Visitor& visitor, const VisitTypeInput& input) const {
    return visitor.visit(this, input);
}

LLVMTypeRef UniversalType::llvm_type() const {
    assert(false);
    return LLVMVoidType();
}

void UniversalType::print(std::ostream& stream) const {
    stream << "\"?\"";
}

#pragma endregion UniversalType

#pragma region IntegerType

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

const IntegerType* IntegerType::uintptr_type() {
    return of(IntegerSignedness::UNSIGNED, IntegerSize::LONG_LONG);
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

VisitTypeOutput IntegerType::accept(Visitor& visitor, const VisitTypeInput& input) const {
    return visitor.visit(this, input);
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
        stream << "\"C\"";
    } else {
        static const char* const signednesses[unsigned(IntegerSignedness::NUM)] = { nullptr, "S", "U" };
        static const char* const sizes[unsigned(IntegerSize::NUM)] = { "b", "c", "s", "i", "l", "m" };
        stream << '"' << signednesses[unsigned(signedness)] << sizes[unsigned(size)] << '"';
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

#pragma endregion IntegerType

#pragma region FloatingPointType

FloatingPointType::FloatingPointType(FloatingPointSize size)
    : size(size) {
}

const FloatingPointType* FloatingPointType::of(FloatingPointSize size) {
    static const FloatingPointType types[int(FloatingPointSize::NUM)] = {
        FloatingPointType(FloatingPointSize::FLOAT),
        FloatingPointType(FloatingPointSize::DOUBLE),
        FloatingPointType(FloatingPointSize::LONG_DOUBLE),
    };

    assert(size < FloatingPointSize::NUM);
    return &types[int(size)];
}

VisitTypeOutput FloatingPointType::accept(Visitor& visitor, const VisitTypeInput& input) const {
    return visitor.visit(this, input);
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
    stream << "\"F" << types[unsigned(size)] << '"';
}

#pragma endregion FloatingPointType

#pragma region PointerType

VisitTypeOutput PointerType::accept(Visitor& visitor, const VisitTypeInput& input) const {
    return visitor.visit(this, input);
}

LLVMTypeRef PointerType::cache_llvm_type() const {
    return LLVMPointerType(base_type->llvm_type(), 0);
}

void PointerType::print(std::ostream& stream) const {
    stream << "[\"P\", " << base_type << ']';
}

PointerType::PointerType(const Type* base_type)
    : base_type(base_type) {
}

#pragma endregion PointerType

#pragma region QualifierType

const Type* QualifiedType::of(const Type* base_type, unsigned qualifiers) {
    if (qualifiers == 0) return base_type;

    while (auto qualified_base_type = dynamic_cast<const QualifiedType*>(base_type)) {
        base_type = qualified_base_type;
        qualifiers |= qualified_base_type->qualifier_flags;
    }

    return TranslationUnitContext::it->type.get_qualified_type(base_type, qualifiers);
}

unsigned QualifiedType::qualifiers() const {
    return qualifier_flags;
}

const Type* QualifiedType::unqualified() const {
    return base_type;
}

bool QualifiedType::is_complete() const {
    return base_type->is_complete();
}

VisitTypeOutput QualifiedType::accept(Visitor& visitor, const VisitTypeInput& input) const {
    return visitor.visit(this, input);
}

LLVMTypeRef QualifiedType::llvm_type() const {
    return base_type->llvm_type();
}

void QualifiedType::print(std::ostream& stream) const {
    stream << "[\"Q";
    if (qualifier_flags & QUAL_CONST) stream << 'c';
    if (qualifier_flags & QUAL_RESTRICT) stream << 'r';
    if (qualifier_flags & QUAL_VOLATILE) stream << 'v';
    stream << "\", " << base_type << ']';
}

QualifiedType::QualifiedType(const Type* base_type, unsigned qualifiers)
    : base_type(base_type), qualifier_flags(qualifiers) {
}

#pragma endregion QualifierType

#pragma region UnqualifiedType

UnqualifiedType::UnqualifiedType(const Type* base_type): base_type(base_type) {
}

unsigned UnqualifiedType::qualifiers() const {
    return 0;
}

const Type* UnqualifiedType::unqualified() const {
    // May only be called on resolved types.
    assert(false);
    return this;
}

VisitTypeOutput UnqualifiedType::accept(Visitor& visitor, const VisitTypeInput& input) const {
    return visitor.visit(this, input);
}

void UnqualifiedType::print(std::ostream& stream) const {
    stream << "[\"Qx\", " << base_type << ']';
}

#pragma endregion UnqualifiedType
#pragma region FunctionType

static void print_function_type(ostream& stream, const Type* return_type, const std::vector<const Type*>& parameter_types, bool variadic) {
    stream << "[\"F\", " << return_type;

    for (auto type: parameter_types) {
        stream << ", " << type;
    }

    if (variadic) stream << ", \"?\"";
    stream << ']';
}

const FunctionType* FunctionType::of(const Type* return_type, std::vector<const Type*> parameter_types, bool variadic) {
    stringstream key_stream;
    print_function_type(key_stream, return_type, parameter_types, variadic);

    auto type = static_cast<const FunctionType*>(TranslationUnitContext::it->type.lookup_indexed_type(key_stream.str()));
    if (type) return type;

    type = new FunctionType(return_type, move(parameter_types), variadic);
    TranslationUnitContext::it->type.add_indexed_type(key_stream.str(), type);
    return type;
}

bool FunctionType::is_complete() const {
    return false;
}

VisitTypeOutput FunctionType::accept(Visitor& visitor, const VisitTypeInput& input) const {
    return visitor.visit(this, input);
}

LLVMTypeRef FunctionType::cache_llvm_type() const {
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

#pragma endregion FunctionType

#pragma region StructuredType

StructuredType::StructuredType(const Location& location)
    : location(location) {
}

const Declarator* StructuredType::lookup_member(const Identifier& identifier) const {
    auto it = member_index.find(identifier.name);
    if (it == member_index.end()) return nullptr;
    return it->second;
}

bool StructuredType::is_complete() const {
    return complete;
}

bool StructuredType::has_tag(const Declarator* declarator) const {
    return tag == declarator;
}

void StructuredType::print(std::ostream& stream) const {
    stream << '[';

    auto separator = false;
    for (auto member : members) {
        if (separator) stream << ", ";
        separator = true;
        stream << "[\"" << member->identifier << "\", " << member->type;
        if (auto entity = member->entity()) {
            if (entity->bit_field_size) {
                stream << ", " << entity->bit_field_size;
            }
        }
        stream << ']';
    }

    if (!complete) {
        if (separator) stream << ", ";
        stream << "\"?\"";
    }

    stream << ']';
}

#pragma endregion StructuredType

#pragma region StructType

StructType::StructType(const Location& location)
    : StructuredType(location) {
}

const Type* StructType::compose_type_def_types(const Type* o) const {
    auto other = static_cast<const StructuredType*>(o);

    if (complete && other->complete) {
        for (size_t i = 0; i < members.size(); ++i) {
            auto declarator1 = members[i];
            auto declarator2 = other->members[i];

            if (declarator1->identifier != declarator2->identifier) return nullptr;
            if (!::compose_type_def_types(declarator1->type, declarator2->type)) return nullptr;

            // TODO bitfield size, etc
        }
        return this;
    }

    return (complete || !other->complete) ? this : other;
}

VisitTypeOutput StructType::accept(Visitor& visitor, const VisitTypeInput& input) const {
    return visitor.visit(this, input);
}

LLVMTypeRef StructType::cache_llvm_type() const {
    vector<LLVMTypeRef> member_types;
    member_types.reserve(members.size());
    for (auto member: members) {
        if (auto member_entity = member->entity()) {
            member_types.push_back(member->type->llvm_type());
        }
    }
    return LLVMStructType(member_types.data(), member_types.size(), false);
}

void StructType::print(std::ostream& stream) const {
    stream << "[\"STRUCT\", ";
    StructuredType::print(stream);
    stream << ']';
}

#pragma endregion StructType

#pragma region UnionType

UnionType::UnionType(const Location& location)
    : StructuredType(location) {
}

static bool union_has_member(const StructuredType* type, const Declarator* find) {
    auto member = type->lookup_member(find->identifier);
    for (auto member: type->members) {
        if (member->identifier == find->identifier &&
            compose_type_def_types(member->type, find->type)) { // TODO: bit field size
            return true;
        }
    }
    return false;
}

VisitTypeOutput UnionType::accept(Visitor& visitor, const VisitTypeInput& input) const {
    return visitor.visit(this, input);
}

const Type* UnionType::compose_type_def_types(const Type* o) const {
    auto other = static_cast<const StructuredType*>(o);

    if (complete) {
        for (auto member: other->members) {
            if (!union_has_member(this, member)) return nullptr;
        }
    }

    if (other->complete) {
        for (auto member: members) {
            if (!union_has_member(other, member)) return nullptr;
        }
    }

    return complete ? this : other;
}

LLVMTypeRef UnionType::cache_llvm_type() const {
    assert(false);
    return nullptr;
}

void UnionType::print(std::ostream& stream) const {
    stream << "[\"UNION\", ";
    StructuredType::print(stream);
    stream << ']';
}

#pragma endregion UnionType


#pragma region EnumType

EnumType::EnumType(const Location& location)
    : location(location) {
}

bool EnumType::is_complete() const {
    return complete;
}

bool EnumType::has_tag(const Declarator* declarator) const {
    return tag;
}

VisitTypeOutput EnumType::accept(Visitor& visitor, const VisitTypeInput& input) const {
    return visitor.visit(this, input);
}

LLVMTypeRef EnumType::llvm_type() const {
    return base_type->llvm_type();
}

void EnumType::add_constant(EnumConstant* constant) {
    auto inserted = constant_index.insert(make_pair(constant->declarator->identifier.name, constant));
    assert(inserted.second);
    constants.push_back(constant);
}

const EnumConstant* EnumType::lookup_constant(const Identifier& identifier) const {
    auto it = constant_index.find(identifier.name);
    if (it == constant_index.end()) return nullptr;
    return it->second;
}

const Type* EnumType::compose_type_def_types(const Type* o) const {
    auto other = static_cast<const EnumType*>(o);

    if (complete) {
        for (auto constant: other->constants) {
            if (constant != lookup_constant(constant->declarator->identifier)) return nullptr;
        }
    }

    if (other->complete) {
        for (auto constant: constants) {
            if (constant != other->lookup_constant(constant->declarator->identifier)) return nullptr;
        }
    }

    return (complete || !other->complete) ? this : other;
}

void EnumType::print(std::ostream& stream) const {
    stream << "[\"ENUM\", [";

    auto separator = false;
    for (auto constant : constants) {
        if (separator) stream << ", ";
        separator = true;
        stream << constant;
    }
    
    if (!complete) {
        if (separator) stream << ", ";
        stream << "\"?\"";
    }

    stream << "]]";
}

#pragma endregion EnumType

#pragma region TypeOfType

TypeOfType::TypeOfType(Expr* expr, const Location& location)
    : location(location), expr(expr) {
}

bool TypeOfType::is_complete() const {
    assert(false);  // should not be called on unresolved type
    return false;
}

VisitTypeOutput TypeOfType::accept(Visitor& visitor, const VisitTypeInput& input) const {
    return visitor.visit(this, input);
}

void TypeOfType::print(std::ostream& stream) const {
    stream << "[\"typeof\", " << expr << "]";
}

#pragma endregion TypeOfTyppe

#pragma region UnboundType

const UnboundType* UnboundType::of(const Identifier& identifier) {
    return TranslationUnitContext::it->type.get_unbound_type(identifier);
}

VisitTypeOutput UnboundType::accept(Visitor& visitor, const VisitTypeInput& input) const {
    return visitor.visit(this, input);
}

LLVMTypeRef UnboundType::llvm_type() const {
    assert(false);
    return nullptr;
}

void UnboundType::print(ostream& stream) const {
    stream << "\"N" << *identifier.name << '"';
}

UnboundType::UnboundType(const Identifier& identifier)
    : identifier(identifier) {
}

#pragma endregion UnboundType

#pragma region TypeDefType

TypeDefType::TypeDefType(Declarator* declarator): declarator(declarator) {
}

const Type* TypeDefType::unqualified() const {
    // May only be called on resolved types.
    assert(false);
    return this;
}

VisitTypeOutput TypeDefType::accept(Visitor& visitor, const VisitTypeInput& input) const {
    return visitor.visit(this, input);
}

LLVMTypeRef TypeDefType::llvm_type() const {
    assert(false);
    return nullptr;
}

void TypeDefType::print(ostream& stream) const {
    stream << declarator->type;
}

#pragma endregion TypeDefType