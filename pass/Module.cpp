#include "Module.h"
#include "parse/Declaration.h"
#include "LLVM.h"
#include "parse/Type.h"
#include "TranslationUnitContext.h"
#include "Utility.h"

Module::Module(const EmitOptions& options): options(options) {
    auto context = TranslationUnitContext::it;
    llvm_module = LLVMModuleCreateWithNameInContext("my_module", context->llvm_context);
    builder = LLVMCreateBuilderInContext(context->llvm_context);
}

Module::~Module() {
    LLVMDisposeBuilder(builder);
    LLVMDisposeModule(llvm_module);
}

LLVMValueRef Module::default_value(const Type* type) {
    auto it = default_values.find(type);
    if (it != default_values.end()) return it->second;

    auto llvm_type = type->llvm_type();
        
    LLVMValueRef value{};
    if (auto struct_type = unqualified_type_cast<StructType>(type->unqualified())) {
        vector<LLVMValueRef> values;
        for (auto declaration: struct_type->declarations) {
            for (auto member: declaration->declarators) {
                if (auto member_variable = member->variable()) {
                    if (member_variable->initializer) {
                        values.push_back(fold_initializer(member->type, member_variable->initializer).get_const());
                    } else {
                        values.push_back(default_value(member->type));
                    }
                }
            }
        }

        value = LLVMConstNamedStruct(llvm_type, values.data(), values.size());
            
    } else {
        value = LLVMConstNull(llvm_type);
    }

    return default_values[type] = value;
}

TypedFunctionRef Module::destructor_wrapper(const StructuredType* type, LLVMValueRef default_value) {
    auto context = TranslationUnitContext::it;

    auto it = destructor_wrappers.find(type);
    if (it != destructor_wrappers.end()) return it->second;

    auto function = type->destructor->function();

    auto old_block = LLVMGetInsertBlock(builder);

    LLVMTypeRef param_types[] = {
        context->llvm_pointer_type,
    };
    TypedFunctionRef ref;
    ref.type = LLVMFunctionType(context->llvm_void_type, param_types, 1, false);
    ref.function = LLVMAddFunction(llvm_module, "destructor_wrapper", ref.type);
    destructor_wrappers[type] = ref;

    if (!options.emit_helpers) return ref;

    LLVMSetLinkage(ref.function, LLVMInternalLinkage);
    LLVMAddAttributeAtIndex(ref.function, 1, nocapture_attribute());

    auto llvm_type = type->llvm_type();
    auto receiver = LLVMGetParam(ref.function, 0);

    auto entry_block = LLVMAppendBasicBlockInContext(context->llvm_context, ref.function, "");
    LLVMPositionBuilderAtEnd(builder, entry_block);

    auto current_state = LLVMBuildLoad2(builder, llvm_type, receiver, "");
    auto indeterminate = indeterminate_bool();
    auto selected = LLVMBuildSelect(builder, indeterminate.dangerously_get_value(builder, EmitOutcome::IR),
                                              current_state,
                                              default_value, "");

    auto is_const = call_is_constant_intrinsic(builder, selected, type->llvm_type());

    auto destructor_block = LLVMAppendBasicBlockInContext(context->llvm_context, ref.function, "");
    auto skip_block = LLVMAppendBasicBlockInContext(context->llvm_context, ref.function, "");

    LLVMBuildCondBr(builder, is_const.dangerously_get_value(builder, EmitOutcome::IR), skip_block, destructor_block);

    LLVMPositionBuilderAtEnd(builder, destructor_block);
    LLVMBuildCall2(builder, type->destructor->type->llvm_type(), function->value.dangerously_get_address(), &receiver, 1, "");
    LLVMBuildBr(builder, skip_block);

    LLVMPositionBuilderAtEnd(builder, skip_block);
    LLVMBuildRetVoid(builder);

    LLVMPositionBuilderAtEnd(builder, old_block);

    return ref;
}

Value Module::indeterminate_bool() {
    auto context = TranslationUnitContext::it;

    if (cached_indeterminate_bool.is_valid()) return cached_indeterminate_bool;

    cached_indeterminate_bool = Value(ValueKind::LVALUE, IntegerType::of_bool(),
                                      LLVMAddGlobal(llvm_module, context->llvm_bool_type, "indeterminate"));
    return cached_indeterminate_bool;
}

TypedFunctionRef Module::lookup_intrinsic(const char* name, LLVMTypeRef* param_types, unsigned num_params) {
    auto context = TranslationUnitContext::it;

    TypedFunctionRef ref;
    auto id = LLVMLookupIntrinsicID(name, strlen(name));
    ref.function = LLVMGetIntrinsicDeclaration(llvm_module, id, param_types, num_params);
    ref.type = LLVMIntrinsicGetType(context->llvm_context, id, param_types, num_params);
    return ref;
}

void Module::call_assume_intrinsic(LLVMBuilderRef builder, LLVMValueRef true_value) {
    auto function = lookup_intrinsic("llvm.assume", nullptr, 0);

    LLVMValueRef args[] = {
        true_value,
    };
    function.call(builder, args, std::size(args));
}

void Module::call_expect_i1_intrinsic(LLVMBuilderRef builder, LLVMValueRef actual_value, LLVMValueRef expected_value) {
    auto context = TranslationUnitContext::it;

    LLVMTypeRef param_types[] = {
        context->llvm_bool_type,
    };
    auto function = lookup_intrinsic("llvm.expect", param_types, std::size(param_types));

    LLVMValueRef args[] = {
        actual_value,
        expected_value,
    };
    function.call(builder, args, std::size(args));
}

Value Module::call_is_constant_intrinsic(LLVMBuilderRef builder, LLVMValueRef value, LLVMTypeRef type) {
    auto function = lookup_intrinsic("llvm.is.constant", &type, 1);
    return Value(IntegerType::of_bool(), function.call(builder, &value, 1));
}

void Module::call_sideeffect_intrinsic(LLVMBuilderRef builder) {
    auto function = lookup_intrinsic("llvm.sideeffect", nullptr, 0);
    function.call(builder, nullptr, 0);
}

unsigned Module::get_enum_attribute_kind(const char* name) {
    return LLVMGetEnumAttributeKindForName(name, strlen(name));
}

LLVMAttributeRef Module::create_enum_attribute(const char* name) {
    auto context = TranslationUnitContext::it;
    auto kind = get_enum_attribute_kind(name);
    return LLVMCreateEnumAttribute(context->llvm_context, kind, 0);
}

LLVMAttributeRef Module::dereferenceable_attribute(uint64_t size) {
    auto context = TranslationUnitContext::it;
    if (!cached_dereferenceable_kind) {
        cached_dereferenceable_kind = get_enum_attribute_kind("dereferenceable");
    }
    return LLVMCreateEnumAttribute(context->llvm_context, cached_dereferenceable_kind, size);
}

LLVMAttributeRef Module::noalias_attribute() {
    if (cached_noalias_attribute) return cached_noalias_attribute;
    return cached_noalias_attribute = create_enum_attribute("noalias");
}

LLVMAttributeRef Module::nocapture_attribute() {
    if (cached_nocapture_attribute) return cached_nocapture_attribute;
    return cached_nocapture_attribute = create_enum_attribute("nocapture");
}

LLVMAttributeRef Module::nonnull_attribute() {
    if (cached_nonnull_attribute) return cached_nonnull_attribute;
    return cached_nonnull_attribute = create_enum_attribute("nonnull");
}

LLVMAttributeRef Module::noundef_attribute() {
    if (cached_nonnull_attribute) return cached_nonnull_attribute;
    return cached_nonnull_attribute = create_enum_attribute("noundef");
}

LLVMAttributeRef Module::readonly_attribute() {
    if (cached_readonly_attribute) return cached_readonly_attribute;
    return cached_readonly_attribute = create_enum_attribute("readonly");
}

void Module::middle_end_passes(const char* passes) {
    auto pass_builder_options = LLVMCreatePassBuilderOptions();
    SCOPE_EXIT {
        LLVMDisposePassBuilderOptions(pass_builder_options);
    };

    LLVMRunPasses(llvm_module, passes, g_llvm_target_machine, pass_builder_options);
}

void Module::back_end_passes() {
    char* error{};
    LLVMTargetMachineEmitToFile(g_llvm_target_machine, llvm_module, "generated.asm", LLVMAssemblyFile, &error);
    LLVMDisposeMessage(error);
}
