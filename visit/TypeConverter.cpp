#include "TypeConverter.h"
#include "Emitter.h"
#include "Message.h"
#include "TypeVisitor.h"

template <typename T, typename U>
const T* type_cast(const U* type) {
    assert(type->qualifiers() == 0);
    return dynamic_cast<const T*>(type);
}

ConvKind check_pointer_conversion(const Type* source_base_type, const Type* dest_base_type) {
    auto unqualified_source_base_type = source_base_type->unqualified();
    auto unqualified_dest_base_type = dest_base_type->unqualified();

    ConvKind result = unqualified_source_base_type == unqualified_dest_base_type ? ConvKind::IMPLICIT : ConvKind::C_IMPLICIT;

    if (unqualified_dest_base_type == &VoidType::it && source_base_type->partition() != TypePartition::FUNCTION) {
        result = ConvKind::IMPLICIT;
    }

    if (result == ConvKind::IMPLICIT) {
        if (auto source_base_pointer_type = type_cast<PointerType>(source_base_type->unqualified())) {
            if (auto dest_base_pointer_type = type_cast<PointerType>(dest_base_type->unqualified())) {
                result = check_pointer_conversion(source_base_pointer_type, dest_base_pointer_type);
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

struct TypeConverter: TypeVisitor {
    Module* module;
    LLVMBuilderRef builder;
    EmitOutcome outcome;
    Value value;
    const Type* dest_type{};
    ConvertTypeResult result;

    LLVMValueRef get_rvalue(const Value &value) {
        return value.get_rvalue(builder, outcome);
    }

    virtual const Type* visit(const ResolvedArrayType* source_type) override {
        if (auto dest_array_type = type_cast<ResolvedArrayType>(dest_type)) {
            if (value.is_const() && source_type->element_type == dest_array_type->element_type && source_type->size <= dest_array_type->size) {
                LLVMValueRef source_array = value.get_const();
                vector<LLVMValueRef> resized_array_values(dest_array_type->size);
                size_t i;
                for (i = 0; i < source_type->size; ++i) {
                    resized_array_values[i] = LLVMGetAggregateElement(source_array, i);
                }
                LLVMValueRef null_value = LLVMConstNull(source_type->element_type->llvm_type());
                for (; i < dest_array_type->size; ++i) {
                    resized_array_values[i] = null_value;
                }

                // TODO: LLVMConstArray2
                LLVMValueRef resized_array = LLVMConstArray(source_type->element_type->llvm_type(), resized_array_values.data(), resized_array_values.size());
                result = ConvertTypeResult(dest_type, resized_array);
            }
        } else if (auto pointer_type = type_cast<PointerType>(dest_type)) {
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
        
        if (value.kind == ValueKind::LVALUE) {
            auto pointer_type = source_type->element_type->pointer_to();
            value = Value(pointer_type, value.get_lvalue());
            visit(pointer_type);
        }

        return nullptr;
    }

    virtual const Type* visit(const EnumType* source_type) override {
        source_type->base_type->accept(*this);
        return nullptr;
    }

    virtual const Type* visit(const FloatingPointType* source_type) override {
        if (type_cast<FloatingPointType>(dest_type)) {
            result = ConvertTypeResult(dest_type, LLVMBuildFPCast(builder, get_rvalue(value), dest_type->llvm_type(), ""));
        } else if (auto dest_int_type = type_cast<IntegerType>(dest_type)) {
            if (dest_int_type->is_signed()) {
                result = ConvertTypeResult(dest_type, LLVMBuildFPToSI(builder, get_rvalue(value), dest_type->llvm_type(), ""));
            } else {
                result = ConvertTypeResult(dest_type, LLVMBuildFPToUI(builder, get_rvalue(value), dest_type->llvm_type(), ""));
            }
        }
        return nullptr;
    }

    virtual const Type* visit(const FunctionType* source_type) override {
        if (auto pointer_type = type_cast<PointerType>(dest_type)) {
            ConvKind kind = pointer_type->base_type->unqualified() == source_type ? ConvKind::IMPLICIT : ConvKind::EXPLICIT;
            result = ConvertTypeResult(dest_type, value.get_lvalue(), kind);
        }
        return nullptr;
    }

    virtual const Type* visit(const IntegerType* source_type) override {
        if (auto int_target = type_cast<IntegerType>(dest_type)) {
            result = ConvertTypeResult(dest_type, LLVMBuildIntCast2(builder, get_rvalue(value), dest_type->llvm_type(), source_type->is_signed(), ""));
        } else if (type_cast<FloatingPointType>(dest_type)) {
            if (source_type->is_signed()) {
                result = ConvertTypeResult(dest_type, LLVMBuildSIToFP(builder, get_rvalue(value), dest_type->llvm_type(), ""));
            } else {
                result = ConvertTypeResult(dest_type, LLVMBuildUIToFP(builder, get_rvalue(value), dest_type->llvm_type(), ""));
            }
        } else if (type_cast<PointerType>(dest_type)) {
            result = ConvertTypeResult(dest_type, LLVMBuildIntToPtr(builder, get_rvalue(value), dest_type->llvm_type(), ""), ConvKind::EXPLICIT);
        }
        return nullptr;
    }
    
    const Type* visit(const PointerType* source_type) {
        if (auto dest_pointer_type = type_cast<PointerType>(dest_type)) {
            result = ConvertTypeResult(value.bit_cast(dest_type), check_pointer_conversion(source_type->base_type, dest_pointer_type->base_type));
        } else if (auto dest_int_type = type_cast<IntegerType>(dest_type)) {
            auto kind = dest_int_type->size == IntegerSize::BOOL ? ConvKind::IMPLICIT : ConvKind::EXPLICIT;
            result = ConvertTypeResult(dest_type, LLVMBuildPtrToInt(builder, get_rvalue(value), dest_type->llvm_type(), ""), kind);
        }
        return nullptr;
    }
    
    const Type* visit(const VoidType* source_type) {
        return nullptr;
    }
};

ConvertTypeResult convert_to_type(const Value& value, const Type* dest_type, Module* module, LLVMBuilderRef builder, EmitOutcome outcome) {
    assert(dest_type->qualifiers() == 0);

    if (dest_type == &VoidType::it) {
        return ConvertTypeResult(Value(dest_type));
    }

    const EnumType* dest_enum_type{};
    if (dest_enum_type = type_cast<EnumType>(dest_type)) {
        dest_type = dest_enum_type->base_type;
    }

    TypeConverter converter;
    ConvertTypeResult& result = converter.result;
    converter.module = module;
    converter.builder = builder;
    converter.outcome = outcome;
    converter.value = value;
    converter.dest_type = dest_type;

    value.type->accept(converter);

    if (dest_enum_type) {
        result.value = result.value.bit_cast(dest_enum_type);
        dest_type = dest_enum_type;
    }

    return converter.result;
}
