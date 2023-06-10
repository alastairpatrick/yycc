#include "Module.h"
#include "DepthFirstVisitor.h"
#include "Message.h"
#include "parse/Declaration.h"
#include "TranslationUnitContext.h"
#include "Utility.h"

// This pass creates LLVM globals for all variables with static duration and LLVM functions, including those nested within functions.

struct EntityPass1: DepthFirstVisitor {
    string prefix;
    LLVMModuleRef llvm_module{};

    virtual VisitDeclaratorOutput visit(Declarator* primary, Variable* entity, const VisitDeclaratorInput& input) override {
        primary = primary->primary;
        entity = primary->variable();
        
        if (entity->storage_duration != StorageDuration::STATIC) return VisitDeclaratorOutput();

        if (entity->value.kind == ValueKind::LVALUE) return VisitDeclaratorOutput();

        auto reference_type = unqualified_type_cast<ReferenceType>(primary->type->unqualified());
        if (reference_type) return VisitDeclaratorOutput();

        auto prefixed_name(prefix);
        prefixed_name += *primary->identifier;

        auto global = LLVMAddGlobal(llvm_module, primary->type->llvm_type(), prefixed_name.c_str());
        entity->value = Value(ValueKind::LVALUE, primary->type, global);

        LLVMSetGlobalConstant(global, primary->type->qualifiers() & QUALIFIER_CONST);

        if (entity->linkage != Linkage::EXTERNAL) {
            LLVMSetLinkage(global, LLVMInternalLinkage);
        }

        return VisitDeclaratorOutput();
    }

    virtual VisitDeclaratorOutput visit(Declarator* primary, Function* entity, const VisitDeclaratorInput& input) override {
        primary = primary->primary;
        entity = primary->function();
        
        if (entity->value.kind == ValueKind::LVALUE) return VisitDeclaratorOutput();

        auto prefixed_name(prefix);
        prefixed_name += *primary->identifier;

        auto llvm_function = LLVMAddFunction(llvm_module, prefixed_name.c_str(), primary->type->llvm_type());
        entity->value = Value(ValueKind::LVALUE, primary->type, llvm_function);

        EntityPass1 pass;
        pass.llvm_module = llvm_module;
        pass.prefix = prefixed_name + '.';

        pass.accept_statement(entity->body);

        return VisitDeclaratorOutput();
    }
};

struct EntityPass2: DepthFirstVisitor {
    virtual VisitDeclaratorOutput visit(Declarator* primary, Variable* variable, const VisitDeclaratorInput& input) override {
        auto context = TranslationUnitContext::it;

        primary = primary->primary;
        variable = primary->variable();
        
        if (variable->storage_duration != StorageDuration::STATIC) return VisitDeclaratorOutput();

        if (variable->value.kind == ValueKind::LVALUE) return VisitDeclaratorOutput();

        auto reference_type = unqualified_type_cast<ReferenceType>(primary->type->unqualified());
        if (!reference_type) return VisitDeclaratorOutput();

        auto value = fold_expr(variable->initializer);

        if (!can_bind_reference_to_value(reference_type, value, primary, variable->initializer->location)) {
            value = Value(ValueKind::LVALUE, reference_type->base_type, context->llvm_null);
        }

        variable->value = value;

        return VisitDeclaratorOutput();
    }
};

void Module::entity_pass() {
    EntityPass1 pass;
    pass.llvm_module = llvm_module;

    Identifier destructor_id = { .text = intern_string("destructor") };

    pass.accept_scope(file_scope);
    for (auto scope: type_scopes) {
        if (scope->type) {
            if (auto destructor = scope->lookup_member(destructor_id)) {
                if (auto function = destructor->function()) {
                    if (function->parameters.size() == 1 && function->parameters[0]->type->unqualified() == ReferenceType::of(scope->type, ReferenceType::Kind::LVALUE)) {
                        scope->type->destructor = destructor;
                    }
                }
            }
        }

        pass.accept_scope(scope);
    }

    EntityPass2 pass2;
    pass2.accept_module(this);
}
