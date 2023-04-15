#include "ResolvePass.h"
#include "Message.h"
#include "parser/Declaration.h"
#include "parser/ArrayType.h"
#include "parser/Type.h"
#include "Visitor.h"


const Type* ResolvePass::resolve(const Type* type) {
    return type->accept(*this, VisitTypeInput()).type;
}



static bool is_trivially_cyclic(Declarator* declarator, const Type* type) {
    for (;;) {
        if (auto qt = dynamic_cast<const QualifiedType*>(type)) {
            type = qt->base_type;
        } else if (auto tdt = dynamic_cast<const TypeDefType*>(type)) {
            return tdt->declarator == declarator;
        } else {
            return false;
        }
    }
}

const Type* ResolvePass::resolve(Declarator* primary) {
    struct ResolutionCycle {};

    if (primary->status == ResolutionStatus::RESOLVED) return primary->type;
    if (primary->status == ResolutionStatus::RESOLVING) throw ResolutionCycle();

    primary->status = ResolutionStatus::RESOLVING;

    Declarator* acyclic_declarator{};
    for (auto declarator = primary; declarator; declarator = declarator->next) {
        try {
            if (declarator->type->has_tag(declarator) && declarator->type->is_complete()) {
                swap(declarator->type, primary->type);
                acyclic_declarator = primary;
                primary->status = ResolutionStatus::RESOLVED;
                primary->type = resolve(primary->type);
                break;
            }

            declarator->type = resolve(declarator->type);
            if (!acyclic_declarator || (declarator->type->is_complete() && !acyclic_declarator->type->is_complete())) {
                acyclic_declarator = declarator;
            }
        } catch (ResolutionCycle) {
            if (!is_trivially_cyclic(primary, declarator->type)) {
                message(Severity::ERROR, declarator->location) << "recursive definition of '" << declarator->identifier << "'\n";
                pause_messages();
            }
        }
    }

    if (primary->type_def()) primary->status = ResolutionStatus::RESOLVED;

    if (acyclic_declarator) {
        swap(acyclic_declarator->type, primary->type);
        assert(!dynamic_cast<const TypeDefType*>(primary->type));

        for (auto secondary = primary->next; secondary; secondary = secondary->next) {
            try {
                secondary->type = resolve(secondary->type);
            } catch (ResolutionCycle) {
                message(Severity::ERROR, secondary->location) << "recursive definition of '" << secondary->identifier << "'\n";
                pause_messages();
            }
            compose(primary, secondary);
        }
    } else {
        auto declarator = primary;
        while (declarator->next) {
            declarator = declarator->next;
        }
        primary->type = IntegerType::default_type();
        message(Severity::ERROR, declarator->location) << "'" << declarator->identifier << "' undeclared\n";
    }

    primary->next = nullptr;
    primary->status = ResolutionStatus::RESOLVED;

    return primary->type;
}

void ResolvePass::compose(Declarator* primary, Declarator* secondary) {
    if (secondary->delegate && primary->delegate && typeid(*secondary->delegate) != typeid(*primary->delegate)) {
        message(Severity::ERROR, secondary->location) << "redeclaration of '" << primary->identifier << "' with different type\n";
        message(Severity::INFO, primary->location) << "see prior declaration\n";
    }

    if (primary->delegate) {
        primary->accept(*this, VisitDeclaratorInput(secondary));
    } else {
        assert(false);
        primary->delegate = secondary->delegate;
    }
}

VisitDeclaratorOutput ResolvePass::visit(Declarator* primary, Entity* primary_entity, const VisitDeclaratorInput& input) {
    auto secondary = input.secondary;

    auto composite_type = compose_types(primary->type, secondary->type);
    if (composite_type) {
        primary->type = composite_type;
    } else {
        message(Severity::ERROR, secondary->location) << "redeclaration of '" << primary->identifier << "' with incompatible type\n";
        message(Severity::INFO, primary->location) << "see prior declaration\n";
    }

    auto secondary_entity = secondary->entity();
    if (!secondary_entity) return VisitDeclaratorOutput();

    if (secondary_entity->initializer) {
        if (primary_entity->initializer) {
            message(Severity::ERROR, secondary->location) << "redefinition of '" << primary->identifier << "'\n";
            message(Severity::INFO, primary->location) << "see prior definition\n";
        } else {
            primary_entity->initializer = secondary_entity->initializer;
        }
    }
    
    if (secondary_entity->body) {
        if (primary_entity->body) {
            message(Severity::ERROR, secondary->location) << "redefinition of '" << primary->identifier << "'\n";
            message(Severity::INFO, primary->location) << "see prior definition\n";
        } else {
            primary_entity->body = secondary_entity->body;
            primary_entity->params = move(secondary_entity->params);
        }
    }
  
    primary_entity->inline_definition = secondary_entity->inline_definition && primary_entity->inline_definition;

    assert(secondary_entity->storage_duration() == primary_entity->storage_duration());

    return VisitDeclaratorOutput();
}

VisitDeclaratorOutput ResolvePass::visit(Declarator* primary, TypeDef* primary_type_def, const VisitDeclaratorInput& input) {
    auto secondary = input.secondary;

    auto composed = compose_type_def_types(primary->type, secondary->type);
    if (!composed) {
        message(Severity::ERROR, secondary->location) << "redefinition of '" << primary->identifier << "' with different type\n";
        message(Severity::INFO, primary->location) << "see other definition\n";
        return VisitDeclaratorOutput();
    }

    assert(primary->type == composed);

    return VisitDeclaratorOutput();
}

VisitTypeOutput ResolvePass::visit_default(const Type* type, const VisitTypeInput& input) {
    return VisitTypeOutput(type->resolve(*this));
}
