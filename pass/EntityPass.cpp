#include "DepthFirstVisitor.h"
#include "parse/Declaration.h"
#include "ResolvePass.h"

// This pass creates LLVM globals for all variables with static duration and LLVM functions, including those nested within functions.
struct EntityPass: DepthFirstVisitor {
    string prefix;
    LLVMModuleRef llvm_module{};

    virtual VisitDeclaratorOutput visit(Declarator* primary, Variable* entity, const VisitDeclaratorInput& input) override {
        primary = primary->primary;
        entity = primary->variable();
        
        if (entity->storage_duration != StorageDuration::STATIC) return VisitDeclaratorOutput();

        if (entity->value.kind == ValueKind::LVALUE) return VisitDeclaratorOutput();

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

        EntityPass pass;
        pass.llvm_module = llvm_module;
        pass.prefix = prefixed_name + '.';

        pass.accept_statement(entity->body);

        return VisitDeclaratorOutput();
    }

    void accept_scope(const Scope* scope) {
        for (auto declarator: scope->declarators) {
            accept_declarator(declarator);
        }
    }
};

void entity_pass(Module& module) {
    EntityPass pass;
    pass.llvm_module = module.llvm_module;

    Identifier destructor_id = { .text = intern_string("destructor") };

    pass.accept_scope(module.file_scope);
    for (auto scope: module.type_scopes) {
        if (scope->type) {
            if (auto destructor = scope->lookup_member(destructor_id)) {
                if (auto function = destructor->function()) {
                    if (function->parameters.size() == 1 && function->parameters[0]->type->unqualified() == PassByReferenceType::of(scope->type)) {
                        scope->type->destructor = destructor;
                    }
                }
            }
        }

        pass.accept_scope(scope);
    }
}
