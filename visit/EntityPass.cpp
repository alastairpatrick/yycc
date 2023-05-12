#include "parse/Declaration.h"
#include "ResolvePass.h"
#include "Visitor.h"

// This pass creates LLVM globals for all variables with static duration and LLVM functions, including those nested within functions.
struct EntityPass: DepthFirstVisitor {
    string prefix;
    LLVMModuleRef llvm_module{};

    void emit(const Scope* scope) {
        for (auto declarator: scope->declarators) {
            accept(declarator, VisitDeclaratorInput());
        }
    }

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

        pass.accept(entity->body);

        return VisitDeclaratorOutput();
    }
};

void entity_pass(const ResolvedModule& resolved_module, LLVMModuleRef llvm_module) {
    EntityPass pass;
    pass.llvm_module = llvm_module;

    pass.emit(resolved_module.file_scope);
    for (auto scope: resolved_module.type_scopes) {
        pass.emit(scope);
    }
}
