#include "ValueWrangler.h"
#include "Module.h"
#include "Message.h"
#include "TranslationUnitContext.h"

ValueWrangler::ValueWrangler(Module* module, EmitOutcome outcome)
    : module(module), outcome(outcome) {
    auto llvm_context = TranslationUnitContext::it->llvm_context;
    if (outcome != EmitOutcome::TYPE) {
        builder = LLVMCreateBuilderInContext(llvm_context);
    }
}

ValueWrangler::~ValueWrangler() {
    if (builder) LLVMDisposeBuilder(builder);
}

ConvKind ValueWrangler::check_pointer_conversion(const Type* source_base_type, const Type* dest_base_type) {
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

    if ((result == ConvKind::IMPLICIT) && (dest_base_type->qualifiers() < source_base_type->qualifiers())) {
        result = ConvKind::C_IMPLICIT;
    }

    if (source_base_type->partition() != dest_base_type->partition()) {
        if (source_base_type->partition() == TypePartition::FUNCTION) result = ConvKind::EXPLICIT;
        if (dest_base_type->partition() == TypePartition::FUNCTION) result = ConvKind::EXPLICIT;
    }

    return result;
}

LLVMValueRef ValueWrangler::get_rvalue(const Value &value, const Location& location, bool for_move_expr) {
    if (value.type == &VoidType::it) {
        return nullptr;
    }

    auto rvalue = value.dangerously_get_rvalue(builder, outcome);

    if (auto structured_type = unqualified_type_cast<StructuredType>(value.type->unqualified())) {
        if (value.kind == ValueKind::LVALUE && structured_type->destructor) {
            LLVMValueRef lvalue = value.dangerously_get_lvalue();
            LLVMBuildStore(builder, LLVMConstNull(structured_type->llvm_type()), lvalue);

            if (!for_move_expr) {
                message(Severity::ERROR, location) << "lvalue with destructor is not copyable; consider '&&' prefix move operator\n";
            }
        }
    }

    return rvalue;
}

void ValueWrangler::store(const Value& dest, LLVMValueRef source_rvalue, const Location& location) {
    if (outcome == EmitOutcome::IR) {
        if (dest.kind == ValueKind::LVALUE) {
            dest.dangerously_store(builder, source_rvalue);
        } else {
            message(Severity::ERROR, location) << "expression is not assignable\n";
        }
    } else {
        message(Severity::ERROR, location) << "assignment in constant expression\n";
    }
}


LLVMValueRef ValueWrangler::get_rvalue(const Value &value) {
    return get_rvalue(value, location, false);
}

void ValueWrangler::convert_array_to_pointer() {
    if (auto source_type = unqualified_type_cast<ResolvedArrayType>(value.type)) {
        if (value.kind == ValueKind::LVALUE) {
            auto pointer_type = source_type->element_type->pointer_to();
            value = Value(pointer_type, value.dangerously_get_lvalue());
        }
    }
}

void ValueWrangler::convert_enum_to_int() {
    if (auto source_type = unqualified_type_cast<EnumType>(value.type)) {
        value = value.bit_cast(source_type->base_type);
    }
}

const Type* ValueWrangler::visit(const ResolvedArrayType* dest_type) {
    if (auto source_type = unqualified_type_cast<ResolvedArrayType>(value.type)) {
        if (value.is_const() && source_type->element_type == dest_type->element_type && source_type->size <= dest_type->size) {
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
            result = ConvertTypeResult(dest_type, resized_array);
        }
    }
    return nullptr;
}

const Type* ValueWrangler::visit(const PointerType* dest_type) {
    if (auto source_type = unqualified_type_cast<ResolvedArrayType>(value.type)) {
        if (module && value.is_const()) {
            auto& global = module->reified_constants[value.get_const()];
            if (!global) {
                global = LLVMAddGlobal(module->llvm_module, value.type->llvm_type(), "const");
                LLVMSetGlobalConstant(global, true);
                LLVMSetLinkage(global, LLVMPrivateLinkage);
                LLVMSetInitializer(global, value.get_const());
            }
            result = ConvertTypeResult(dest_type, global);
            return nullptr;
        }
    }
        
    convert_array_to_pointer();

    if (auto source_type = unqualified_type_cast<FunctionType>(value.type)) {
        ConvKind kind = dest_type->base_type->unqualified() == source_type ? ConvKind::IMPLICIT : ConvKind::EXPLICIT;
        result = ConvertTypeResult(dest_type, value.dangerously_get_lvalue(), kind);
    } else if (auto source_type = unqualified_type_cast<IntegerType>(value.type)) {
        result = ConvertTypeResult(dest_type, LLVMBuildIntToPtr(builder, get_rvalue(value), dest_type->llvm_type(), ""), ConvKind::EXPLICIT);
    } else if (auto source_type = unqualified_type_cast<PointerType>(value.type)) {
        result = ConvertTypeResult(value.bit_cast(dest_type), check_pointer_conversion(source_type->base_type, dest_type->base_type));
    }

    return nullptr;
}

const Type* ValueWrangler::visit(const IntegerType* dest_type) {
    convert_array_to_pointer();
    convert_enum_to_int();

    if (auto source_type = unqualified_type_cast<IntegerType>(value.type)) {
        result = ConvertTypeResult(dest_type, LLVMBuildIntCast2(builder, get_rvalue(value), dest_type->llvm_type(), source_type->is_signed(), ""));
    } else if (auto source_type = unqualified_type_cast<FloatingPointType>(value.type)) {
        if (dest_type->is_signed()) {
            result = ConvertTypeResult(dest_type, LLVMBuildFPToSI(builder, get_rvalue(value), dest_type->llvm_type(), ""));
        } else {
            result = ConvertTypeResult(dest_type, LLVMBuildFPToUI(builder, get_rvalue(value), dest_type->llvm_type(), ""));
        }
    } else if (auto source_type = unqualified_type_cast<PointerType>(value.type)) {
        auto kind = dest_type->size == IntegerSize::BOOL ? ConvKind::IMPLICIT : ConvKind::EXPLICIT;
        result = ConvertTypeResult(dest_type, LLVMBuildPtrToInt(builder, get_rvalue(value), dest_type->llvm_type(), ""), kind);
    }

    return nullptr;
}

const Type* ValueWrangler::visit(const FloatingPointType* dest_type) {
    convert_enum_to_int();

    if (unqualified_type_cast<FloatingPointType>(value.type)) {
        result = ConvertTypeResult(dest_type, LLVMBuildFPCast(builder, get_rvalue(value), dest_type->llvm_type(), ""));
    } else if (auto source_type = unqualified_type_cast<IntegerType>(value.type)) {
        if (source_type->is_signed()) {
            result = ConvertTypeResult(dest_type, LLVMBuildSIToFP(builder, get_rvalue(value), dest_type->llvm_type(), ""));
        } else {
            result = ConvertTypeResult(dest_type, LLVMBuildUIToFP(builder, get_rvalue(value), dest_type->llvm_type(), ""));
        }
    }
    return nullptr;
}

const Type* ValueWrangler::visit(const EnumType* dest_type) {
    dest_type->base_type->accept(*this);
    if (result.value.is_valid()) {
        result.value = result.value.bit_cast(dest_type);
        result.conv_kind = ConvKind::EXPLICIT;
    }
    return nullptr;
}

const Type* ValueWrangler::visit(const VoidType* dest_type) {
    result = ConvertTypeResult(Value(dest_type));
    return nullptr;
}


ConvertTypeResult ValueWrangler::convert_to_type(const Value& value, const Type* dest_type, ConvKind kind, const Location& location) {
    assert(value.type->qualifiers() == 0);
        
    this->value = value;
    this->location = location;

    dest_type = dest_type->unqualified();

    if (value.is_null_literal && kind == ConvKind::IMPLICIT && unqualified_type_cast<PointerType>(dest_type)) {
        return ConvertTypeResult(dest_type, LLVMConstNull(dest_type->llvm_type()));
    }

    if (value.type == dest_type) {
        Value result = value;
        if (kind == ConvKind::EXPLICIT) result.is_null_literal = false;
        return ConvertTypeResult(result);
    }

    dest_type->accept(*this);
    
    if (!result.value.is_valid()) {
        message(Severity::ERROR, location) << "cannot convert from type '" << PrintType(value.type)
                                            << "' to type '" << PrintType(dest_type) << "'\n";
        pause_messages();
        if (dest_type != &VoidType::it) {
            result.value = Value(dest_type, LLVMConstNull(dest_type->llvm_type()));
        } else {
            result.value = Value::of_zero_int();
        }
    } else if ((result.conv_kind != ConvKind::IMPLICIT) && kind == ConvKind::IMPLICIT) {
        auto severity = result.conv_kind == ConvKind::C_IMPLICIT ? Severity::CONTEXTUAL_ERROR : Severity::ERROR;
        message(severity, location) << "conversion from type '" << PrintType(value.type)
                                    << "' to type '" << PrintType(dest_type) << "' requires explicit cast\n";
    }

    assert(result.value.type == dest_type);

    return result;
}
