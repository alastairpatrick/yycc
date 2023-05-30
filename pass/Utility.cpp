#include "Utility.h"

#include "parse/Constant.h"

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

    // todo: fix, e.g. const int & < volatile int& yet discards qualifiers 
    if ((result == ConvKind::IMPLICIT) && (dest_base_type->qualifiers() < source_base_type->qualifiers())) {
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

static bool follow_geps(LLVMValueRef haystack, LLVMValueRef needle) {
    for (;;) {
        if (needle == haystack) return true;

        auto opcode = LLVMGetInstructionOpcode(haystack);
        if (opcode != LLVMGetElementPtr) return false;

        haystack = LLVMGetOperand(haystack, 0);
    }
}

bool values_are_aliases(LLVMValueRef a, LLVMValueRef b) {
    return follow_geps(a, b) || follow_geps(b, a);
}
