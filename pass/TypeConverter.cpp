#include "TypeConverter.h"
#include "Module.h"
#include "Message.h"
#include "TranslationUnitContext.h"
#include "Utility.h"

// todo: hopefully the only way to make a compile time "array constant" is with a string literal?
bool check_array_constant_conversion(const Type* source_element_type, const Type* dest_element_type) {
    dest_element_type = dest_element_type->unqualified();
    source_element_type = source_element_type->unqualified();

    if (source_element_type == dest_element_type) return true;

    // C99 6.7.8p14,15
    auto dest_int_el_type = unqualified_type_cast<IntegerType>(dest_element_type);
    auto source_int_el_type = unqualified_type_cast<IntegerType>(source_element_type);
    return dest_int_el_type && source_int_el_type && dest_int_el_type->size == source_int_el_type->size;
}

TypeConverter::TypeConverter(Module* module, LLVMBuilderRef builder, EmitOutcome outcome, ValueResolver& resolver)
    : module(module), builder(builder), outcome(outcome), resolver(resolver) {
}

LLVMValueRef TypeConverter::get_value_internal() {
    return resolver.get_value(value, false);
}

void TypeConverter::convert_array_to_pointer() {
    if (auto source_type = unqualified_type_cast<ResolvedArrayType>(value.type)) {
        if (value.kind == ValueKind::LVALUE) {
            auto pointer_type = source_type->element_type->pointer_to();
            value = ExprValue(pointer_type, value.dangerously_get_address(), value.node);
        }
    }
}

void TypeConverter::convert_enum_to_int() {
    if (auto source_type = unqualified_type_cast<EnumType>(value.type)) {
        value = value.bit_cast(source_type->base_type);
    }
}

const Type* TypeConverter::visit(const ResolvedArrayType* dest_type) {
    if (auto source_type = unqualified_type_cast<ResolvedArrayType>(value.type)) {
        if (value.is_const() && check_array_constant_conversion(source_type->element_type, dest_type->element_type) && source_type->size <= dest_type->size) {
            LLVMValueRef source_array = value.get_const();
            vector<LLVMValueRef> resized_array_values(dest_type->size);
            size_t i;
            for (i = 0; i < source_type->size; ++i) {
                resized_array_values[i] = LLVMGetAggregateElement(source_array, i);
            }
            LLVMValueRef null_value = LLVMConstNull(source_type->element_type->llvm_type());
            for (; i < dest_type->size; ++i) {
                resized_array_values[i] = null_value;
            }

            // TODO: LLVMConstArray2
            LLVMValueRef resized_array = LLVMConstArray(source_type->element_type->llvm_type(), resized_array_values.data(), resized_array_values.size());
            result = Value(dest_type, resized_array);
        }
    }
    return nullptr;
}

const Type* TypeConverter::visit(const PointerType* dest_type) {
    if (auto source_type = unqualified_type_cast<ResolvedArrayType>(value.type)) {
        if (module && value.is_const()) {
            conv_kind = check_pointer_conversion(source_type->element_type, dest_type->base_type);

            if (check_array_constant_conversion(source_type->element_type, dest_type->base_type)) {
                conv_kind = ConvKind::IMPLICIT;
            }

            auto& global = module->reified_constants[value.get_const()];
            if (!global) {
                global = LLVMAddGlobal(module->llvm_module, value.type->llvm_type(), "const");
                LLVMSetGlobalConstant(global, true);
                LLVMSetLinkage(global, LLVMPrivateLinkage);
                LLVMSetInitializer(global, value.get_const());
            }

            result = Value(dest_type, global);
            return nullptr;
        }
    }
        
    convert_array_to_pointer();

    if (auto source_type = unqualified_type_cast<FunctionType>(value.type)) {
        conv_kind = dest_type->base_type->unqualified() == source_type ? ConvKind::IMPLICIT : ConvKind::EXPLICIT;
        result = Value(dest_type, value.dangerously_get_address());
    } else if (auto source_type = unqualified_type_cast<IntegerType>(value.type)) {
        conv_kind = ConvKind::EXPLICIT;
        result = Value(dest_type, LLVMBuildIntToPtr(builder, get_value_internal(), dest_type->llvm_type(), ""));
    } else if (auto source_type = unqualified_type_cast<PointerType>(value.type)) {
        conv_kind = check_pointer_conversion(source_type->base_type, dest_type->base_type);
        result = Value(value.bit_cast(dest_type));
    }

    return nullptr;
}

const Type* TypeConverter::visit(const IntegerType* dest_type) {
    convert_array_to_pointer();
    convert_enum_to_int();

    if (auto source_type = unqualified_type_cast<IntegerType>(value.type)) {
        result = Value(dest_type, LLVMBuildIntCast2(builder, get_value_internal(), dest_type->llvm_type(), source_type->is_signed(), ""));
    } else if (auto source_type = unqualified_type_cast<FloatingPointType>(value.type)) {
        if (dest_type->is_signed()) {
            result = Value(dest_type, LLVMBuildFPToSI(builder, get_value_internal(), dest_type->llvm_type(), ""));
        } else {
            result = Value(dest_type, LLVMBuildFPToUI(builder, get_value_internal(), dest_type->llvm_type(), ""));
        }
    } else if (auto source_type = unqualified_type_cast<PointerType>(value.type)) {
        conv_kind = dest_type->size == IntegerSize::BOOL ? ConvKind::IMPLICIT : ConvKind::EXPLICIT;
        result = Value(dest_type, LLVMBuildPtrToInt(builder, get_value_internal(), dest_type->llvm_type(), ""));
    }

    return nullptr;
}

const Type* TypeConverter::visit(const FloatingPointType* dest_type) {
    convert_enum_to_int();

    if (unqualified_type_cast<FloatingPointType>(value.type)) {
        result = Value(dest_type, LLVMBuildFPCast(builder, get_value_internal(), dest_type->llvm_type(), ""));
    } else if (auto source_type = unqualified_type_cast<IntegerType>(value.type)) {
        if (source_type->is_signed()) {
            result = Value(dest_type, LLVMBuildSIToFP(builder, get_value_internal(), dest_type->llvm_type(), ""));
        } else {
            result = Value(dest_type, LLVMBuildUIToFP(builder, get_value_internal(), dest_type->llvm_type(), ""));
        }
    }
    return nullptr;
}

const Type* TypeConverter::visit(const EnumType* dest_type) {
    dest_type->base_type->accept(*this);
    if (result.is_valid()) {
        result = result.bit_cast(dest_type);
        conv_kind = ConvKind::EXPLICIT;
    }
    return nullptr;
}

const Type* TypeConverter::visit(const VoidType* dest_type) {
    result = Value(dest_type);
    return nullptr;
}


ExprValue TypeConverter::convert_to_type(ExprValue value, const Type* dest_type, ConvKind kind) {
    assert(value.type->qualifiers() == 0);
    
    this->value = value;
    conv_kind = ConvKind::IMPLICIT;

    dest_type = dest_type->unqualified();

    if (value.is_null_literal && kind == ConvKind::IMPLICIT && unqualified_type_cast<PointerType>(dest_type)) {
        return ExprValue(dest_type, LLVMConstNull(dest_type->llvm_type()), value.node);
    }

    if (value.type == dest_type) {
        ExprValue result = value;
        if (kind == ConvKind::EXPLICIT) result.is_null_literal = false;
        return result;
    }

    dest_type->accept(*this);
    
    if (!result.is_valid()) {
        message(Severity::ERROR, value.node->location) << "cannot convert from type '" << value.message_type()
                                                       << "' to type '" << PrintType(dest_type) << "'\n";
        pause_messages();
        if (dest_type != &VoidType::it) {
            result = Value(dest_type, LLVMConstNull(dest_type->llvm_type()));
        } else {
            result = Value::of_zero_int();
        }
    } else if ((conv_kind != ConvKind::IMPLICIT) && kind == ConvKind::IMPLICIT) {
        auto severity = conv_kind == ConvKind::C_IMPLICIT ? Severity::CONTEXTUAL_ERROR : Severity::ERROR;
        message(severity, value.node->location) << "conversion from type '" << value.message_type()
                                                << "' to type '" << PrintType(dest_type) << "' requires explicit cast\n";
    }

    assert(result.type == dest_type);

    return ExprValue(result, value.node);
}
