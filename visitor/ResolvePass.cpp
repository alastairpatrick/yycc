#include "ResolvePass.h"
#include "Message.h"
#include "parser/Declaration.h"
#include "parser/ArrayType.h"
#include "parser/Expr.h"
#include "parser/IdentifierMap.h"
#include "parser/Type.h"
#include "Visitor.h"

struct ResolvePass: Visitor {
    unordered_set<Declarator*> todo;

    const Type* resolve(const Type* type) {
        return type->accept(*this, VisitTypeInput()).value.type;
    }

    void resolve(Statement* statement) {
        statement->accept(*this, VisitStatementInput());
    }

    bool is_trivially_cyclic(Declarator* declarator, const Type* type) {
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

    const Type* resolve(Declarator* primary) {
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

    void compose(Declarator* primary, Declarator* secondary) {
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

    virtual VisitDeclaratorOutput visit(Declarator* primary, Entity* primary_entity, const VisitDeclaratorInput& input) override {
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

    virtual VisitDeclaratorOutput visit(Declarator* primary, TypeDef* primary_type_def, const VisitDeclaratorInput& input) override {
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

    virtual VisitTypeOutput visit_default(const Type* type, const VisitTypeInput& input) override {
        return VisitTypeOutput(type);
    }

    virtual VisitTypeOutput visit(const PointerType* type, const VisitTypeInput& input) override {
        return VisitTypeOutput(resolve(type->base_type)->pointer_to());
    }

    virtual VisitTypeOutput visit(const QualifiedType* type, const VisitTypeInput& input) override {
        return VisitTypeOutput(QualifiedType::of(resolve(type->base_type), type->qualifier_flags));
    }

    virtual VisitTypeOutput visit(const UnqualifiedType* type, const VisitTypeInput& input) override {
        return VisitTypeOutput(resolve(type->base_type)->unqualified());
    }

    virtual VisitTypeOutput visit(const FunctionType* type, const VisitTypeInput& input) override {
        auto resolved_return_type = resolve(type->return_type);
        auto resolved_param_types(type->parameter_types);
        for (auto& param_type : resolved_param_types) {
            param_type = resolve(param_type);

            // C99 6.7.5.3p7
            if (auto array_type = dynamic_cast<const ArrayType*>(param_type->unqualified())) {
                param_type = QualifiedType::of(array_type->element_type->pointer_to(), param_type->qualifiers());
            }

            // C99 6.7.5.3p8
            if (auto function_type = dynamic_cast<const FunctionType*>(param_type)) {
                param_type = param_type->pointer_to();
            }

        }
        return VisitTypeOutput(FunctionType::of(resolved_return_type, resolved_param_types, type->variadic));
    }

    virtual VisitTypeOutput visit(const StructType* type, const VisitTypeInput& input) override {
        return visit_structured_type(type, input);
    }

    virtual VisitTypeOutput visit(const UnionType* type, const VisitTypeInput& input) override {
        return visit_structured_type(type, input);
    }

    VisitTypeOutput visit_structured_type(const StructuredType* type, const VisitTypeInput& input) {
        auto want_complete = type->complete;
        type->complete = false;

        for (auto member: type->members) {
            resolve(member);

            if (auto member_entity = member->entity()) {
                if (!member->type->is_complete()) {
                    message(Severity::ERROR, member->location) << "member '" << member->identifier << "' has incomplete type\n";
                }
            }
        }

        type->complete = want_complete;
        return VisitTypeOutput(type);
    }

    virtual VisitTypeOutput visit(const EnumType* type, const VisitTypeInput& input) override {
        auto want_complete = type->complete;
        type->complete = false;

        type->base_type = IntegerType::default_type();
        long long next_int = 0;

        for (auto constant: type->constants) {
            resolve(constant->declarator);
            if (constant->constant_expr) {
                resolve(constant->constant_expr);
                auto value = constant->constant_expr->fold();
                next_int = LLVMConstIntGetSExtValue(value.value);
            }

            constant->constant_int = next_int;
            ++next_int;
        }

        type->complete = want_complete;
        return VisitTypeOutput(type);
    }

    virtual VisitTypeOutput visit(const TypeOfType* type, const VisitTypeInput& input) override {
        resolve(type->expr);
        return VisitTypeOutput(type->expr->get_type());
    }

    virtual VisitTypeOutput visit(const TypeDefType* type, const VisitTypeInput& input) override {
        return VisitTypeOutput(resolve(type->declarator));
    }

    virtual VisitTypeOutput visit(const UnresolvedArrayType* type, const VisitTypeInput& input) override {
        auto resolved_element_type = resolve(type->element_type);
        if (!resolved_element_type->is_complete()) {
            message(Severity::ERROR, type->location) << "incomplete array element type\n";
            resolved_element_type = IntegerType::default_type();
        }

        if (type->size) {
            resolve(type->size);
            auto size_constant = type->size->fold();
            unsigned long long size_int = 1;
            if (!size_constant.is_const_integer()) {
                message(Severity::ERROR, type->size->location) << "size of array must have integer type\n";
            } else {
                size_int = LLVMConstIntGetZExtValue(size_constant.value);
            }

            return VisitTypeOutput(ResolvedArrayType::of(ArrayKind::COMPLETE, resolved_element_type, size_int));
        } else {
            return VisitTypeOutput(ResolvedArrayType::of(ArrayKind::INCOMPLETE, resolved_element_type, 0));
        }
    }

    virtual VisitStatementOutput visit(EntityExpr* expr, const VisitStatementInput& input) override {
        resolve(expr->declarator);
        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(SizeOfExpr* expr, const VisitStatementInput& input) override {
        expr->type = resolve(expr->type);
        return VisitStatementOutput();
    }
};

void resolve_pass(const IdentifierMap::Scope& scope) {
    ResolvePass pass;
    for (auto p: scope.declarators) {
        pass.todo.insert(p.second);
    }

    while (pass.todo.size()) {
        auto it = pass.todo.begin();
        auto declarator = *it;
        pass.todo.erase(it);

        resume_messages();
        pass.resolve(declarator);
        resume_messages();
    }
}
