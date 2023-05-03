#include "Type.h"

#include "ArrayType.h"
#include "Constant.h"
#include "Declaration.h"
#include "Expr.h"
#include "IdentifierMap.h"
#include "InternedString.h"
#include "Message.h"
#include "TranslationUnitContext.h"
#include "visit/Visitor.h"
#include "visit/Emitter.h"

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

// Prints type in three consecutive sections:
//  0: all parts that don't relate to a declarator, e.g. int
//  1: pointer asterisks, opening perentheses and qualifiers
//  2: array and function parts and closing parentheses
ostream& operator<<(ostream& stream, const PrintType& print_type) {
    for (int i = 0; i < 3; ++i) {
        print_type.type->message_print(stream, i);
    }
    return stream;
}


QualifierSet Type::qualifiers() const {
    return 0;
}

const Type* Type::unqualified() const {
    return this;
}

LLVMTypeRef Type::llvm_type() const {
    assert(false);
    return nullptr;
}

LLVMTypeRef CachedType::llvm_type() const {
    if (cached_llvm_type) return cached_llvm_type;
    return cached_llvm_type = cache_llvm_type();
}

const PointerType* Type::pointer_to() const {
    return TranslationUnitContext::it->type.get_pointer_type(this);
}

TypePartition Type::partition() const {
    return TypePartition::OBJECT;
}

bool Type::has_tag(const Declarator* declarator) const {
    return false;
}



const VoidType VoidType::it;

TypePartition VoidType::partition() const {
    return TypePartition::INCOMPLETE;
}

VisitTypeOutput VoidType::accept(Visitor& visitor, const VisitTypeInput& input) const {
    return visitor.visit(this, input);
}

LLVMTypeRef VoidType::llvm_type() const {
    auto llvm_context = TranslationUnitContext::it->llvm_context;
    return LLVMVoidTypeInContext(llvm_context);
}

void VoidType::message_print(ostream& stream, int section) const {
    if (section == 0) {
        stream << "void";
    }
}

void VoidType::print(std::ostream& stream) const {
    stream << "\"V\"";
}



const IntegerType* IntegerType::of_bool() {
    return IntegerType::of(IntegerSignedness::UNSIGNED, IntegerSize::BOOL);
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

const IntegerType* IntegerType::of_size(IntegerSignedness signedness) {
    return IntegerType::of(signedness, IntegerSize::LONG_LONG);
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

int IntegerType::num_bits() const {
    unsigned long long result{};
    switch (size) {
      default:
        assert(false);
        return 0;
      case IntegerSize::BOOL:
        return 1;
      case IntegerSize::CHAR:
        return 8;
      case IntegerSize::SHORT:
        return 16;
      case IntegerSize::INT:
      case IntegerSize::LONG:
        return 32;
      case IntegerSize::LONG_LONG:
        return 64;
    }
}

unsigned long long IntegerType::max() const {
    auto result = ~0ULL >> (num_bits() - 64);
    if (signedness == IntegerSignedness::SIGNED) result >>= 1;
    return result;
}

VisitTypeOutput IntegerType::accept(Visitor& visitor, const VisitTypeInput& input) const {
    return visitor.visit(this, input);
}

LLVMTypeRef IntegerType::llvm_type() const {
    auto llvm_context = TranslationUnitContext::it->llvm_context;
    switch (size) {
      default:
        assert(false);
        return nullptr;
      case IntegerSize::BOOL:
        return LLVMInt1TypeInContext(llvm_context);
      case IntegerSize::CHAR:
        return LLVMInt8TypeInContext(llvm_context);
      case IntegerSize::SHORT:
        return LLVMInt16TypeInContext(llvm_context);
      case IntegerSize::INT:
        return LLVMInt32TypeInContext(llvm_context);
      case IntegerSize::LONG:
        return LLVMInt32TypeInContext(llvm_context);
      case IntegerSize::LONG_LONG:
        return LLVMInt64TypeInContext(llvm_context);
    };
}

void IntegerType::message_print(ostream& stream, int section) const {
    if (section != 0) return;

    if (signedness == IntegerSignedness::DEFAULT) {
        stream << "char";
        return;
    }

    if (signedness == IntegerSignedness::UNSIGNED && size != IntegerSize::BOOL) {
        stream << "unsigned ";
    }

    switch (size) {
      case IntegerSize::BOOL:
        stream << "bool";
        break;
      case IntegerSize::CHAR:
        stream << "char";
        break;
      case IntegerSize::SHORT:
        stream << "short";
        break;
      case IntegerSize::INT:
        stream << "int";
        break;
      case IntegerSize::LONG:
        stream << "long";
        break;
      case IntegerSize::LONG_LONG:
        stream << "long long";
        break;
      default:
        assert(false);
        break;
    }
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
    auto llvm_context = TranslationUnitContext::it->llvm_context;
    switch (size) {
      default:
        assert(false);
        return nullptr;
      case FloatingPointSize::FLOAT:
        return LLVMFloatTypeInContext(llvm_context);
      case FloatingPointSize::DOUBLE:
        return LLVMDoubleTypeInContext(llvm_context);
      case FloatingPointSize::LONG_DOUBLE:
        return LLVMX86FP80TypeInContext(llvm_context);
    };
}

void FloatingPointType::message_print(ostream& stream, int section) const {
    if (section != 0) return;

    switch (size) {
      case FloatingPointSize::FLOAT:
        stream << "float";
        break;
      case FloatingPointSize::DOUBLE:
        stream << "double";
        break;
      case FloatingPointSize::LONG_DOUBLE:
        stream << "long double";
        break;
      default:
        assert(false);
        break;
    }
}

void FloatingPointType::print(ostream& stream) const {
    static const char* const types[int(IntegerSize::NUM)] = { "f", "d", "l" };
    stream << "\"F" << types[unsigned(size)] << '"';
}



VisitTypeOutput PointerType::accept(Visitor& visitor, const VisitTypeInput& input) const {
    return visitor.visit(this, input);
}

LLVMTypeRef PointerType::cache_llvm_type() const {
    auto llvm_context = TranslationUnitContext::it->llvm_context;
    return LLVMPointerTypeInContext(llvm_context, 0);
}

void PointerType::message_print(ostream& stream, int section) const {
    bool need_paren = dynamic_cast<const ArrayType*>(base_type) || dynamic_cast<const FunctionType*>(base_type);

    if (section == 2) {
        if (need_paren) stream << ')';
    }

    base_type->message_print(stream, section);

    if (section == 1) {
        if (need_paren) {
            stream << "(*";
        } else {
            stream << '*';
        }
    }
}

void PointerType::print(std::ostream& stream) const {
    stream << "[\"P\", " << base_type << ']';
}

PointerType::PointerType(const Type* base_type)
    : base_type(base_type) {
}



const Type* QualifiedType::of(const Type* base_type, unsigned qualifiers) {
    if (qualifiers == 0) return base_type;

    while (auto qualified_base_type = dynamic_cast<const QualifiedType*>(base_type)) {
        base_type = qualified_base_type;
        qualifiers |= qualified_base_type->qualifier_flags;
    }

    return TranslationUnitContext::it->type.get_qualified_type(base_type, qualifiers);
}

QualifierSet QualifiedType::qualifiers() const {
    return qualifier_flags;
}

const Type* QualifiedType::unqualified() const {
    return base_type;
}

TypePartition QualifiedType::partition() const {
    return base_type->partition();
}

VisitTypeOutput QualifiedType::accept(Visitor& visitor, const VisitTypeInput& input) const {
    return visitor.visit(this, input);
}

LLVMTypeRef QualifiedType::llvm_type() const {
    return base_type->llvm_type();
}

void QualifiedType::message_print(ostream& stream, int section) const {
    base_type->message_print(stream, section);

    if (section == 1) {
        if (qualifier_flags & QUALIFIER_CONST) stream << " const";
        if (qualifier_flags & QUALIFIER_RESTRICT) stream << " restricted";
        if (qualifier_flags & QUALIFIER_VOLATILE) stream << " volatile";
    }
}

void QualifiedType::print(std::ostream& stream) const {
    stream << "[\"Q";
    if (qualifier_flags & QUALIFIER_CONST) stream << 'c';
    if (qualifier_flags & QUALIFIER_RESTRICT) stream << 'r';
    if (qualifier_flags & QUALIFIER_VOLATILE) stream << 'v';
    stream << "\", " << base_type << ']';
}

QualifiedType::QualifiedType(const Type* base_type, unsigned qualifiers)
    : base_type(base_type), qualifier_flags(qualifiers) {
}



UnqualifiedType::UnqualifiedType(const Type* base_type): base_type(base_type) {
}

QualifierSet UnqualifiedType::qualifiers() const {
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

void UnqualifiedType::message_print(ostream& stream, int section) const {
    base_type->message_print(stream, section);
    if (section == 1) stream << " unqualified";
}

void UnqualifiedType::print(std::ostream& stream) const {
    stream << "[\"Qx\", " << base_type << ']';
}



const FunctionType* FunctionType::of(const Type* return_type, std::vector<const Type*> parameter_types, bool variadic) {
    return TranslationUnitContext::it->type.get_function_type(return_type, parameter_types, variadic);
}

TypePartition FunctionType::partition() const {
    return TypePartition::FUNCTION;
}

VisitTypeOutput FunctionType::accept(Visitor& visitor, const VisitTypeInput& input) const {
    return visitor.visit(this, input);
}

LLVMTypeRef FunctionType::cache_llvm_type() const {
    vector<LLVMTypeRef> param_llvm;
    param_llvm.reserve(parameter_types.size());

    for (auto type: parameter_types) param_llvm.push_back(type->llvm_type());

    return LLVMFunctionType(return_type->llvm_type(), param_llvm.data(), unsigned int(param_llvm.size()), variadic);
}

void FunctionType::message_print(ostream& stream, int section) const {
    if (section == 2) {
        stream << '(';
        bool separate{};
        for (auto type: parameter_types) {
            if (separate) stream << ", ";
            separate = true;
            stream << PrintType(type);
        }
        stream << ')';
    }

    return_type->message_print(stream, section);
}

void FunctionType::print(std::ostream& stream) const {
    stream << "[\"F\", " << return_type;

    for (auto type: parameter_types) {
        stream << ", " << type;
    }

    if (variadic) stream << ", \"?\"";
    stream << ']';
}

FunctionType::FunctionType(const Type* return_type, std::vector<const Type*> parameter_types, bool variadic)
    : return_type(return_type), parameter_types(move(parameter_types)), variadic(variadic) {
}



TagType::TagType(const Location& location): LocationNode(location) {
}

void TagType::message_print(ostream& stream, int section) const {
    if (section != 0) return;

    if (tag) {
        stream << *tag->identifier.name;
    } else {
        stream << "anon";
    }
}



StructuredType::StructuredType(const Location& location)
    : TagType(location) {
}

const Declarator* StructuredType::lookup_member(const Identifier& identifier) const {
    auto it = scope.declarator_map.find(identifier.name);
    if (it == scope.declarator_map.end()) return nullptr;
    return it->second;
}

TypePartition StructuredType::partition() const {
    return complete ? TypePartition::OBJECT : TypePartition::INCOMPLETE;
}

bool StructuredType::has_tag(const Declarator* declarator) const {
    return tag == declarator;
}

void StructuredType::print(std::ostream& stream) const {
    stream << '[';

    auto separator = false;
    for (auto declaration: declarations) {
        for (auto member : declaration->declarators) {
            if (separator) stream << ", ";
            separator = true;
            stream << "[\"" << member->identifier << "\", " << member->type;
            if (auto member_variable = member->variable()) {
                if (member_variable->member->bit_field) {
                    stream << ", " << member_variable->member->bit_field;
                }
            }
            stream << ']';
        }
    }

    if (!complete) {
        if (separator) stream << ", ";
        stream << "\"?\"";
    }

    stream << ']';
}

LLVMTypeRef StructuredType::build_llvm_struct_type(const vector<LLVMValueRef>& gep_indices_prefix, const char* name) const {
    auto context = TranslationUnitContext::it;
    auto llvm_context = context->llvm_context;
    bool is_union = dynamic_cast<const UnionType*>(this);

    if (name) cached_llvm_type = LLVMStructCreateNamed(llvm_context, name);

    vector<Declarator*> members;
    size_t largest_member_idx{};
    unsigned long long largest_member_size{};
    members.reserve(declarations.size());
    for (auto declaration: declarations) {
        for (auto member: declaration->declarators) {
            if (auto member_variable = member->variable()) {
                if (is_union) {
                    auto member_size = LLVMStoreSizeOfType(context->llvm_target_data, member->type->llvm_type());
                    if (member_size > largest_member_size) {
                        largest_member_size = member_size;
                        largest_member_idx = members.size();
                    }
                }

                members.push_back(member);
            }
        }
    }

    if (is_union && members.size()) {
        swap(members[largest_member_idx], members.back());
    }

    auto gep_index_type = IntegerType::default_type();

    vector<LLVMTypeRef> member_types;
    unsigned aggregate_index{};
    int bits_to_left{};
    int bits_to_right{}; 
    LLVMTypeRef bit_field_type{};

    for (size_t member_idx = 0; member_idx < members.size(); ++member_idx) {
        auto member = members[member_idx];

        if (auto member_variable = member->variable()) {
            member_variable->member->gep_indices = gep_indices_prefix;

            auto bit_field = member_variable->member->bit_field.get();
            if (bit_field) {
                if (auto integer_type = dynamic_cast<const IntegerType*>(member->type)) {
                    auto bit_size_value = fold_expr(member_variable->member->bit_field->expr);
                    if (!bit_size_value.is_const_integer()) {
                        message(Severity::ERROR, bit_field->expr->location) << "bit field '" << member->identifier << "' must have integer type, not '"
                                                                                             << PrintType(bit_size_value.type) << "'\n";
                        continue;
                    }

                    auto llvm_bit_size = bit_size_value.get_const();
                    auto bit_size = LLVMConstIntGetSExtValue(llvm_bit_size);
                    if (bit_size <= 0) {
                        message(Severity::ERROR, bit_field->expr->location) << "bit field '" << member->identifier << "' has invalid width ("
                                                                                             << bit_size << " bits)\n";
                        continue;
                    }

                    if (bit_size > integer_type->num_bits()) {
                        message(Severity::ERROR, bit_field->expr->location) << "width of bit field '" << member->identifier
                                                                                              << "' (" << bit_size << " bits) exceeds width of its type '"
                                                                                              << PrintType(integer_type) << "' (" << integer_type->num_bits() << " bits)\n";
                        continue;
                    }

                    if (!is_union && bit_size <= bits_to_left) {
                        bit_field->storage_type = bit_field_type;
                        bit_field->bits_to_left = LLVMConstInt(bit_field_type, bits_to_left - bit_size, false);
                        bit_field->bits_to_right = LLVMConstInt(bit_field_type, bits_to_right, false);

                        // mask = (all_ones >> (bits_to_left + bits_to_right))) << bits_to_right
                        bit_field->mask = LLVMConstShl(LLVMConstLShr(LLVMConstAllOnes(bit_field_type), LLVMConstAdd(bit_field->bits_to_left, bit_field->bits_to_right)), bit_field->bits_to_right);
                        bits_to_right += bit_size;
                        bits_to_left -= bit_size;

                        member_variable->member->gep_indices.push_back(Value::of_int(gep_index_type, aggregate_index - 1).get_const());
                        if (is_union) member_variable->member->gep_indices.push_back(context->zero_int);

                        continue;
                    }

                    bit_field_type = integer_type->llvm_type();
                    bits_to_right = 0;
                    bits_to_left = integer_type->num_bits();

                    bit_field->storage_type = bit_field_type;
                    bit_field->bits_to_left = LLVMConstInt(bit_field_type, bits_to_left - bit_size, false);
                    bit_field->bits_to_right = LLVMConstInt(bit_field_type, 0, false);

                    // mask = all_ones >> bits_to_left
                    bit_field->mask = LLVMConstLShr(LLVMConstAllOnes(bit_field_type), bit_field->bits_to_left);
                    bits_to_right += bit_size;
                    bits_to_left -= bit_size;

                } else {
                    message(Severity::ERROR, member->location) << "bit field '" << member->identifier << "' has non-integer type '" << PrintType(member->type) << "'\n";
                }
            }

            member_variable->member->gep_indices.push_back(Value::of_int(gep_index_type, aggregate_index).get_const());
            if (is_union) member_variable->member->gep_indices.push_back(context->zero_int);
            ++aggregate_index;
            
            // Anonymous struct or union?
            LLVMTypeRef llvm_member_type{};
            if (member->identifier.name->empty()) {
                if (auto structured_member_type = dynamic_cast<const StructuredType*>(member->type)) {
                    llvm_member_type = structured_member_type->build_llvm_struct_type(member_variable->member->gep_indices, nullptr);
                }
            }

            if (!llvm_member_type) {
                llvm_member_type = member->type->llvm_type();
            }

            if (is_union) {
                // A union is represented as an LLVM struct, with all but the last member being a zero size array of the appropriate type.
                // The last member of the LLVM struct has array size one, and so is the only one contributing to the size of the LLVM struct.
                // It must be the member with the largest storage, which was arranged near the beginning of this function. Because all
                // but the last array have zero size, all the arrays are located beginning at the same address, as needed.
                //
                // union This {           <- C union type
                //   double d;
                //   float f;
                //   char c;
                // };
                // 
                // struct BecomesThis {   <- LLVM struct type
                //   char c[0];           <- error in C but equivalent okay in LLVM type system
                //   float f[0];
                //   double d[1];         <- largest member swapped to last earlier
                // };

                llvm_member_type = LLVMArrayType(llvm_member_type, member_idx == members.size() - 1 ? 1 : 0);
            }

            member_types.push_back(llvm_member_type);
        }
    }

    if (name) {
        LLVMStructSetBody(cached_llvm_type, member_types.data(), member_types.size(), false);
    } else {
        cached_llvm_type = LLVMStructTypeInContext(llvm_context, member_types.data(), member_types.size(), false);
    }

    return cached_llvm_type;
}

LLVMTypeRef StructuredType::cache_llvm_type() const {
    const char* name{};
    if (tag && !tag->identifier.name->empty()) name = tag->identifier.name->data();
    if (!name) name = "anon";

    return build_llvm_struct_type({TranslationUnitContext::it->zero_int}, name);
}


StructType::StructType(const Location& location)
    : StructuredType(location) {
}

VisitTypeOutput StructType::accept(Visitor& visitor, const VisitTypeInput& input) const {
    return visitor.visit(this, input);
}

void StructType::message_print(ostream& stream, int section) const {
    if (section == 0) stream << "struct ";
    StructuredType::message_print(stream, section);
}

void StructType::print(std::ostream& stream) const {
    stream << "[\"STRUCT\", ";
    StructuredType::print(stream);
    stream << ']';
}



UnionType::UnionType(const Location& location)
    : StructuredType(location) {
}

VisitTypeOutput UnionType::accept(Visitor& visitor, const VisitTypeInput& input) const {
    return visitor.visit(this, input);
}

void UnionType::message_print(ostream& stream, int section) const {
    if (section == 0) stream << "union ";
    StructuredType::message_print(stream, section);
}

void UnionType::print(std::ostream& stream) const {
    stream << "[\"UNION\", ";
    StructuredType::print(stream);
    stream << ']';
}



EnumType::EnumType(const Location& location)
    : TagType(location), base_type(IntegerType::default_type()) {
}

TypePartition EnumType::partition() const {
    return complete ? TypePartition::OBJECT : TypePartition::INCOMPLETE;
}

bool EnumType::has_tag(const Declarator* declarator) const {
    return tag;
}

VisitTypeOutput EnumType::accept(Visitor& visitor, const VisitTypeInput& input) const {
    return visitor.visit(this, input);
}

LLVMTypeRef EnumType::cache_llvm_type() const {
    return base_type->llvm_type();
}

void EnumType::message_print(ostream& stream, int section) const {
    if (section == 0) stream << "enum ";
    TagType::message_print(stream, section);
}

void EnumType::print(std::ostream& stream) const {
    stream << "[\"ENUM\", ";
    
    if (explicit_base_type) {
        stream << base_type << ", ";
    }

    stream << "[";

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



TypeOfType::TypeOfType(Expr* expr, const Location& location)
    : location(location), expr(expr) {
}

TypePartition TypeOfType::partition() const {
    assert(false);  // should not be called on unresolved type
    return TypePartition::INCOMPLETE;
}

VisitTypeOutput TypeOfType::accept(Visitor& visitor, const VisitTypeInput& input) const {
    return visitor.visit(this, input);
}

void TypeOfType::message_print(ostream& stream, int section) const {
    if (section == 0) stream << "typeof()";
}

void TypeOfType::print(std::ostream& stream) const {
    stream << "[\"typeof\", " << expr << "]";
}



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

void UnboundType::message_print(ostream& stream, int section) const {
    if (section == 0) stream << identifier;
}

void UnboundType::print(ostream& stream) const {
    stream << "\"N" << identifier << '"';
}

UnboundType::UnboundType(const Identifier& identifier)
    : identifier(identifier) {
}



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

void TypeDefType::message_print(ostream& stream, int section) const {
    if (section == 0) stream << *declarator->identifier.name;
}

void TypeDefType::print(ostream& stream) const {
    stream << declarator->type;
}

