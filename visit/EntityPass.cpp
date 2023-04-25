#include "parse/Declaration.h"
#include "Visitor.h"

static const char* identifier_name(const Identifier& identifier) {
    auto name = identifier.name;
    if (name->empty()) return "";

    return name->data();
}

// This pass creates LLVM globals for all variables with static duration and LLVM functions, including those nested within functions.
struct EntityPass: Visitor {
    std::string prefix;
    LLVMModuleRef llvm_module{};

    void emit(const ASTNodeVector& nodes) {
        for (auto node: nodes) {
            if (auto declaration = dynamic_cast<Declaration*>(node)) {
                for (auto declarator: declaration->declarators) {
                    accept(declarator, VisitDeclaratorInput());
                }
            }
        }
    }

    virtual VisitDeclaratorOutput visit(Declarator* primary, Entity* entity, const VisitDeclaratorInput& input) override {
        primary = primary->primary;
        entity = primary->entity();
        
        if (entity->value.kind == ValueKind::LVALUE) return VisitDeclaratorOutput();

        auto name = identifier_name(primary->identifier);
        auto prefixed_name = prefix + name;

        if (entity->is_function()) {
            auto function = LLVMAddFunction(llvm_module, prefixed_name.c_str(), primary->type->llvm_type());
            entity->value = Value(ValueKind::LVALUE, primary->type, function);

            EntityPass pass;
            pass.llvm_module = llvm_module;
            pass.prefix = prefix + name + '.';
            
            pass.accept(entity->body, VisitStatementInput());

        } else if (entity->storage_duration() == StorageDuration::STATIC) {
            auto global = LLVMAddGlobal(llvm_module, primary->type->llvm_type(), prefixed_name.c_str());
            entity->value = Value(ValueKind::LVALUE, primary->type, global);

            LLVMSetGlobalConstant(global, primary->type->qualifiers() & QUAL_CONST);

            if (entity->linkage() != Linkage::EXTERNAL) {
                LLVMSetLinkage(global, LLVMInternalLinkage);
            }
        }

        return VisitDeclaratorOutput();
    }
};

void entity_pass(const ASTNodeVector& nodes, LLVMModuleRef llvm_module) {
    EntityPass pass;
    pass.llvm_module = llvm_module;
    pass.emit(nodes);
}
