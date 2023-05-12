#include "TypeConverter.h"
#include "Emitter.h"

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

    LLVMValueRef get_rvalue(const Value &value) {
        return value.get_rvalue(builder, outcome);
    }

    VisitTypeOutput visit(const ResolvedArrayType* source_type, const VisitTypeInput& input) {
        auto dest_type = input.dest_type;
        auto value = input.value;

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
                return VisitTypeOutput(dest_type, resized_array);
            }
        }

        if (auto pointer_type = type_cast<PointerType>(dest_type)) {
            if (value.is_const()) {
                auto& global = module->reified_constants[value.get_const()];
                if (!global) {
                    global = LLVMAddGlobal(module->llvm_module, value.type->llvm_type(), "const");
                    LLVMSetGlobalConstant(global, true);
                    LLVMSetLinkage(global, LLVMPrivateLinkage);
                    LLVMSetInitializer(global, value.get_const());
                }
                return VisitTypeOutput(dest_type, global);
            }
        }

        if (value.kind == ValueKind::LVALUE) {
            auto pointer_type = source_type->element_type->pointer_to();
            Value pointer_value(pointer_type, value.get_lvalue());
            return visit(pointer_type, VisitTypeInput(pointer_value, dest_type));
        }

        return VisitTypeOutput(); 
    }

    virtual VisitTypeOutput visit(const EnumType* source_type, const VisitTypeInput& input) override {
        return source_type->base_type->accept(*this, input);
    }

    virtual VisitTypeOutput visit(const FloatingPointType* source_type, const VisitTypeInput& input) override {
        auto dest_type = input.dest_type;
        auto value = input.value;

        if (type_cast<FloatingPointType>(dest_type)) {
            return VisitTypeOutput(dest_type, LLVMBuildFPCast(builder, get_rvalue(value), input.dest_type->llvm_type(), ""));
        }

        if (auto dest_int_type = type_cast<IntegerType>(dest_type)) {
            if (dest_int_type->is_signed()) {
                return VisitTypeOutput(dest_type, LLVMBuildFPToSI(builder, get_rvalue(value), input.dest_type->llvm_type(), ""));
            } else {
                return VisitTypeOutput(dest_type, LLVMBuildFPToUI(builder, get_rvalue(value), input.dest_type->llvm_type(), ""));
            }
        }

        return VisitTypeOutput();
    }

    virtual VisitTypeOutput visit(const FunctionType* source_type, const VisitTypeInput& input) override {
        auto dest_type = input.dest_type;
        auto value = input.value;

        if (auto pointer_type = type_cast<PointerType>(dest_type)) {
            ConvKind kind = pointer_type->base_type->unqualified() == source_type ? ConvKind::IMPLICIT : ConvKind::EXPLICIT;
            return VisitTypeOutput(dest_type, value.get_lvalue(), kind);
        }

        return VisitTypeOutput();
    }

    virtual VisitTypeOutput visit(const IntegerType* source_type, const VisitTypeInput& input) override {
        auto dest_type = input.dest_type;
        auto value = input.value;

        if (auto int_target = type_cast<IntegerType>(dest_type)) {
            return VisitTypeOutput(dest_type, LLVMBuildIntCast2(builder, get_rvalue(value), dest_type->llvm_type(), source_type->is_signed(), ""));
        }

        if (type_cast<FloatingPointType>(dest_type)) {
            if (source_type->is_signed()) {
                return VisitTypeOutput(dest_type, LLVMBuildSIToFP(builder, get_rvalue(value), dest_type->llvm_type(), ""));
            } else {
                return VisitTypeOutput(dest_type, LLVMBuildUIToFP(builder, get_rvalue(value), dest_type->llvm_type(), ""));
            }
        }

        if (type_cast<PointerType>(dest_type)) {
            return VisitTypeOutput(dest_type, LLVMBuildIntToPtr(builder, get_rvalue(value), dest_type->llvm_type(), ""), ConvKind::EXPLICIT);
        }

        return VisitTypeOutput();
    }
    
    VisitTypeOutput visit(const PointerType* source_type, const VisitTypeInput& input) {
        auto dest_type = input.dest_type;
        auto value = input.value;

        if (auto dest_pointer_type = type_cast<PointerType>(dest_type)) {
            return VisitTypeOutput(value.bit_cast(dest_type), check_pointer_conversion(source_type->base_type, dest_pointer_type->base_type));
        }

        if (auto dest_int_type = type_cast<IntegerType>(dest_type)) {
            auto kind = dest_int_type->size == IntegerSize::BOOL ? ConvKind::IMPLICIT : ConvKind::EXPLICIT;
            return VisitTypeOutput(dest_type, LLVMBuildPtrToInt(builder, get_rvalue(value), dest_type->llvm_type(), ""), kind);
        }

        return VisitTypeOutput();
    }
    
    VisitTypeOutput visit(const VoidType* source_type, const VisitTypeInput& input) {
        return VisitTypeOutput();
    }
};

VisitTypeOutput convert_to_type(const VisitTypeInput& input, Module* module, LLVMBuilderRef builder, EmitOutcome outcome) {
    TypeConverter converter;
    converter.module = module;
    converter.builder = builder;
    converter.outcome = outcome;
    return input.value.type->accept(converter, input);
}
