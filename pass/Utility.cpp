#include "Utility.h"
#include "Message.h"
#include "parse/Constant.h"
#include "TranslationUnitContext.h"

std::string message_declarator(const Declarator* declarator) {
    if (!declarator) return "";
    return string(declarator->message_kind()) + " '" + string(*declarator->identifier) + "' of ";
}

bool can_bind_reference_to_value(const ReferenceType* type, Value value, const Declarator* declarator, const Location& location) {
    auto context = TranslationUnitContext::it;

    if (type->kind == ReferenceType::Kind::LVALUE) {
        if (value.kind != ValueKind::LVALUE) {
            message(Severity::ERROR, location) << "cannot bind " << message_declarator(declarator)
                                                << "reference type '" << PrintType(type) << "' to rvalue\n";
            return false;
        }

        if (!value.scoped_lifetime) {
            message(Severity::ERROR, location) << "cannot prove lvalue of type '" << value.message_type()
                                               << "' has scoped lifetime\n";
            return false;
        }
    }
        
    if (type->kind == ReferenceType::Kind::RVALUE && value.kind != ValueKind::RVALUE) {
        message(Severity::ERROR, location) << "cannot bind " << message_declarator(declarator)
                                            << "reference type '" << PrintType(type) << "' to lvalue; consider '&&' move expression\n";
        return false;
    }

    if (type->base_type->unqualified() != value.type->unqualified()) {
        message(Severity::ERROR, location) << "cannot bind " << message_declarator(declarator) << "reference type '"
                                            << PrintType(type) << "' to value of type '" << value.message_type() << "'\n";
        return false;
    }
        
    if (discards_qualifiers(value.qualifiers, type->base_type->qualifiers())) {
        message(Severity::ERROR, location) << "binding " <<message_declarator(declarator)
                                            << "reference type '" << PrintType(type)
                                            << "' to value of type '" << value.message_type() << "' discards type qualifier\n";
        return false;
    }
        
    return true;
}

ConvKind check_pointer_conversion(const Type* source_base_type, const Type* dest_base_type) {
    auto unqualified_source_base_type = source_base_type->unqualified();
    auto unqualified_dest_base_type = dest_base_type->unqualified();

    ConvKind result = unqualified_source_base_type == unqualified_dest_base_type ? ConvKind::IMPLICIT : ConvKind::C_IMPLICIT;

    if (unqualified_dest_base_type == &VoidType::it && source_base_type->partition() != TypePartition::FUNCTION) {
        result = ConvKind::IMPLICIT;
    }

    if (result == ConvKind::IMPLICIT) {
        if (auto source_base_pointer_type = unqualified_type_cast<PointerType>(source_base_type->unqualified())) {
            if (auto dest_base_pointer_type = unqualified_type_cast<PointerType>(dest_base_type->unqualified())) {
                result = check_pointer_conversion(source_base_pointer_type->base_type, dest_base_pointer_type->base_type);
            }
        }
    }

    if ((result == ConvKind::IMPLICIT) && discards_qualifiers(source_base_type->qualifiers(), dest_base_type->qualifiers())) {
        result = ConvKind::C_IMPLICIT;
    }

    if (source_base_type->partition() != dest_base_type->partition()) {
        if (source_base_type->partition() == TypePartition::FUNCTION) result = ConvKind::EXPLICIT;
        if (dest_base_type->partition() == TypePartition::FUNCTION) result = ConvKind::EXPLICIT;
    }

    return result;
}

// C99 6.7.8p14,15
bool is_string_initializer(const ResolvedArrayType* array_type, const InitializerExpr* initializer) {
    auto int_element_type = unqualified_type_cast<IntegerType>(array_type->element_type->unqualified());
    return (int_element_type &&
        initializer->elements.size() == 1 &&
        dynamic_cast<StringConstant*>(initializer->elements[0]));
}

static bool values_are_aliases_internal(LLVMValueRef haystack, LLVMValueRef needle) {
    for (;;) {
        if (needle == haystack) return true;

        auto opcode = LLVMGetInstructionOpcode(haystack);
        if (opcode != LLVMGetElementPtr) return false;

        haystack = LLVMGetOperand(haystack, 0);
    }
}

bool values_are_aliases(LLVMValueRef a, LLVMValueRef b) {
    return values_are_aliases_internal(a, b) || values_are_aliases_internal(b, a);
}
