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

    if (outcome == EmitOutcome::IR) {
        temp_builder = LLVMCreateBuilderInContext(llvm_context);
    }
}

ValueWrangler::~ValueWrangler() {
    if (temp_builder) LLVMDisposeBuilder(temp_builder);
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

LLVMValueRef ValueWrangler::get_address(const Value &value) {
    return value.dangerously_get_address();
}

LLVMValueRef ValueWrangler::get_value(const Value &value, const Location& location, bool for_move_expr) {
    if (value.type == &VoidType::it) {
        return nullptr;
    }

    auto rvalue = value.dangerously_get_value(builder, outcome);

    if (auto structured_type = unqualified_type_cast<StructuredType>(value.type->unqualified())) {
        if (value.kind == ValueKind::LVALUE && structured_type->destructor) {
            LLVMValueRef lvalue = value.dangerously_get_address();
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
    
void ValueWrangler::position_temp_builder() {
    auto first_insn = LLVMGetFirstInstruction(entry_block);
    if (first_insn) {
        LLVMPositionBuilderBefore(temp_builder, first_insn);
    } else {
        LLVMPositionBuilderAtEnd(temp_builder, entry_block);
    }
}

void ValueWrangler::make_addressable(Value& value) {
    position_temp_builder();
    value.make_addressable(temp_builder, builder);
}

Value ValueWrangler::allocate_auto_storage(const Type* type, const char* name) {
    position_temp_builder();
    auto storage = LLVMBuildAlloca(temp_builder, type->llvm_type(), name);
    return Value(ValueKind::LVALUE, type, storage);
}

void ValueWrangler::call_sideeffect_intrinsic() {
    auto function = module->lookup_intrinsic("llvm.sideeffect", nullptr, 0);
    function.call(builder, nullptr, 0);
}

Value ValueWrangler::call_is_constant_intrinsic(const Value& value) {
    auto type = value.type->llvm_type();
    auto function = module->lookup_intrinsic("llvm.is.constant", &type, 1);
    auto arg = value.dangerously_get_value(builder, outcome);
    return Value(IntegerType::of_bool(), function.call(builder, &arg, 1));
}

LLVMValueRef ValueWrangler::get_value_internal(const Value &value) {
    return get_value(value, location, false);
}

void ValueWrangler::convert_array_to_pointer() {
    if (auto source_type = unqualified_type_cast<ResolvedArrayType>(value.type)) {
        if (value.kind == ValueKind::LVALUE) {
            auto pointer_type = source_type->element_type->pointer_to();
            value = Value(pointer_type, value.dangerously_get_address());
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
            result = Value(dest_type, resized_array);
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
        result = Value(dest_type, LLVMBuildIntToPtr(builder, get_value_internal(value), dest_type->llvm_type(), ""));
    } else if (auto source_type = unqualified_type_cast<PointerType>(value.type)) {
        conv_kind = check_pointer_conversion(source_type->base_type, dest_type->base_type);
        result = Value(value.bit_cast(dest_type));
    }

    return nullptr;
}

const Type* ValueWrangler::visit(const IntegerType* dest_type) {
    convert_array_to_pointer();
    convert_enum_to_int();

    if (auto source_type = unqualified_type_cast<IntegerType>(value.type)) {
        result = Value(dest_type, LLVMBuildIntCast2(builder, get_value_internal(value), dest_type->llvm_type(), source_type->is_signed(), ""));
    } else if (auto source_type = unqualified_type_cast<FloatingPointType>(value.type)) {
        if (dest_type->is_signed()) {
            result = Value(dest_type, LLVMBuildFPToSI(builder, get_value_internal(value), dest_type->llvm_type(), ""));
        } else {
            result = Value(dest_type, LLVMBuildFPToUI(builder, get_value_internal(value), dest_type->llvm_type(), ""));
        }
    } else if (auto source_type = unqualified_type_cast<PointerType>(value.type)) {
        conv_kind = dest_type->size == IntegerSize::BOOL ? ConvKind::IMPLICIT : ConvKind::EXPLICIT;
        result = Value(dest_type, LLVMBuildPtrToInt(builder, get_value_internal(value), dest_type->llvm_type(), ""));
    }

    return nullptr;
}

const Type* ValueWrangler::visit(const FloatingPointType* dest_type) {
    convert_enum_to_int();

    if (unqualified_type_cast<FloatingPointType>(value.type)) {
        result = Value(dest_type, LLVMBuildFPCast(builder, get_value_internal(value), dest_type->llvm_type(), ""));
    } else if (auto source_type = unqualified_type_cast<IntegerType>(value.type)) {
        if (source_type->is_signed()) {
            result = Value(dest_type, LLVMBuildSIToFP(builder, get_value_internal(value), dest_type->llvm_type(), ""));
        } else {
            result = Value(dest_type, LLVMBuildUIToFP(builder, get_value_internal(value), dest_type->llvm_type(), ""));
        }
    }
    return nullptr;
}

const Type* ValueWrangler::visit(const EnumType* dest_type) {
    dest_type->base_type->accept(*this);
    if (result.is_valid()) {
        result = result.bit_cast(dest_type);
        conv_kind = ConvKind::EXPLICIT;
    }
    return nullptr;
}

const Type* ValueWrangler::visit(const VoidType* dest_type) {
    result = Value(dest_type);
    return nullptr;
}


Value ValueWrangler::convert_to_type(const Value& value, const Type* dest_type, ConvKind kind, const Location& location) {
    assert(value.type->qualifiers() == 0);
    
    this->value = value;
    this->location = location;
    conv_kind = ConvKind::IMPLICIT;

    dest_type = dest_type->unqualified();

    if (value.is_null_literal && kind == ConvKind::IMPLICIT && unqualified_type_cast<PointerType>(dest_type)) {
        return Value(dest_type, LLVMConstNull(dest_type->llvm_type()));
    }

    if (value.type == dest_type) {
        Value result = value;
        if (kind == ConvKind::EXPLICIT) result.is_null_literal = false;
        return Value(result);
    }

    dest_type->accept(*this);
    
    if (!result.is_valid()) {
        message(Severity::ERROR, location) << "cannot convert from type '" << PrintType(value.type)
                                            << "' to type '" << PrintType(dest_type) << "'\n";
        pause_messages();
        if (dest_type != &VoidType::it) {
            result = Value(dest_type, LLVMConstNull(dest_type->llvm_type()));
        } else {
            result = Value::of_zero_int();
        }
    } else if ((conv_kind != ConvKind::IMPLICIT) && kind == ConvKind::IMPLICIT) {
        auto severity = conv_kind == ConvKind::C_IMPLICIT ? Severity::CONTEXTUAL_ERROR : Severity::ERROR;
        message(severity, location) << "conversion from type '" << PrintType(value.type)
                                    << "' to type '" << PrintType(dest_type) << "' requires explicit cast\n";
    }

    assert(result.type == dest_type);

    return result;
}
