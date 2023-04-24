#include "ResolvePass.h"
#include "Emitter.h"
#include "Message.h"
#include "parse/Declaration.h"
#include "parse/ArrayType.h"
#include "parse/Expr.h"
#include "parse/IdentifierMap.h"
#include "parse/Type.h"
#include "Visitor.h"

struct ResolvePass: Visitor {
    const Type* resolve(const Type* type) {
        return type->accept(*this, VisitTypeInput()).value.type;
    }

    void resolve(Statement* statement) {
        if (!statement) return;

        for (auto& label: statement->labels) {
            resolve(label.case_expr);
        }

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

        primary = primary->primary;
        if (primary->status >= DeclaratorStatus::RESOLVED) return primary->type;
        if (primary->status == DeclaratorStatus::RESOLVING) throw ResolutionCycle();

        primary->status = DeclaratorStatus::RESOLVING;

        Declarator* acyclic_declarator{};
        for (auto declarator = primary; declarator; declarator = declarator->next) {
            try {
                if (declarator->type->has_tag(declarator) && declarator->type->is_complete()) {
                    swap(declarator->type, primary->type);
                    acyclic_declarator = primary;
                    primary->status = DeclaratorStatus::RESOLVED;
                    auto resolved_type = resolve(primary->type);
                    assert(resolved_type == primary->type);  // must be because declarator was already marked resolved
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

        if (primary->type_def()) primary->status = DeclaratorStatus::RESOLVED;

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
        primary->accept(*this, VisitDeclaratorInput());
        primary->status = DeclaratorStatus::RESOLVED;

        return primary->type;
    }
    
    void see_other_message(const Location& location) {
        message(Severity::INFO, location) << "...see other\n";
    }

    void redeclaration_error(const Declarator* secondary, const Location &primary_location, const char* problem) {
        auto& stream = message(Severity::ERROR, secondary->location);
        
        if (secondary->delegate->is_definition()) {
            stream << "redefinition";
        } else {
            stream << "redeclaration";
        }
        
        stream << " of " << secondary->delegate->error_kind() << " '" << secondary->identifier << "'";
        
        if (problem) {
            stream << ' ' << problem;
        }

        stream << "...\n";

        see_other_message(primary_location);
        pause_messages();
    }

    void compose(Declarator* primary, Declarator* secondary) {
        if (secondary->delegate && primary->delegate && typeid(*secondary->delegate) != typeid(*primary->delegate)) {
            redeclaration_error(secondary, primary->location, "with different kind of identifier");
            return;
        }

        if (primary->delegate) {
            primary->accept(*this, VisitDeclaratorInput(secondary));
        } else {
            assert(false);
            primary->delegate = secondary->delegate;
        }
    }
    
    const Type* composite_type(const Type* a, const Type* b) {
        if (a == b) return a;

        if (a == &UniversalType::it) return b;
        if (b == &UniversalType::it) return a;

        if (typeid(*a) == typeid(*b)) {
            if (auto a_array = dynamic_cast<const ResolvedArrayType*>(a)) {
                auto b_array = static_cast<const ResolvedArrayType*>(b);

                if (a_array->element_type != b_array->element_type) return nullptr;

                if (a_array->kind == ArrayKind::INCOMPLETE) return b_array;
                if (b_array->kind == ArrayKind::INCOMPLETE) return a_array;

                if (a_array->size == b_array->size) {
                    assert(a_array == b_array);
                    return a_array;
                }
            }
        }

        return nullptr;
    }

    const Type* compose_array_type_with_initializer_size(const ResolvedArrayType* type, size_t size) {
        return composite_type(type, ResolvedArrayType::of(ArrayKind::COMPLETE, type->element_type, size));
    }

    virtual VisitDeclaratorOutput visit(Declarator* primary, Entity* primary_entity, const VisitDeclaratorInput& input) override {
        auto secondary = input.secondary;
        if (!secondary) {
            auto function_type = dynamic_cast<const FunctionType*>(primary->type);
            for (size_t i = 0; i < primary_entity->parameters.size(); ++i) {
                primary_entity->parameters[i]->type = function_type->parameter_types[i];
                resolve(primary_entity->parameters[i]);
            }

            if (primary_entity->bit_field_size) resolve(primary_entity->bit_field_size);
            if (primary_entity->body) resolve(primary_entity->body);

            if (primary_entity->initializer) {
                resolve(primary_entity->initializer);
                if (auto array_type = dynamic_cast<const ResolvedArrayType*>(primary->type)) {
                    // C99 6.7.8p22
                    if (auto string_constant = dynamic_cast<StringConstant*>(primary_entity->initializer)) {
                        auto string_size = string_constant->value.length + 1;
                        if (auto resolved = compose_array_type_with_initializer_size(array_type, string_size)) {
                            primary->type = resolved;
                        } else if (array_type->kind == ArrayKind::COMPLETE && string_size > array_type->size) {
                            message(Severity::ERROR, string_constant->location) << "size of string literal (" << string_size << ") exceeds declared array size (" << array_type->size << ")\n";
                        }
                    }

                    // C99 6.7.8p22
                    if (auto init_expr = dynamic_cast<InitializerExpr*>(primary_entity->initializer)) {
                        if (auto resolved = compose_array_type_with_initializer_size(array_type, init_expr->elements.size())) {
                            primary->type = resolved;
                        } else if (array_type->kind == ArrayKind::COMPLETE && init_expr->elements.size() > array_type->size) {
                            message(Severity::ERROR, init_expr->elements[array_type->size]->location) << "excess elements in array initializer\n";
                        }
                    }
                }
            }

            if (!function_type && !primary->type->is_complete()) {
                message(Severity::ERROR, primary->location) << "member '" << primary->identifier << "' has incomplete type\n";
            }

            return VisitDeclaratorOutput();
        }

        auto secondary_entity = secondary->entity();
        assert(secondary_entity); //  TODO

        if (primary_entity->linkage() == Linkage::NONE || secondary_entity->linkage() == Linkage::NONE) {
            if (primary->declaration->scope == IdentifierScope::STRUCTURED) {
                message(Severity::ERROR, secondary->location) << "duplicate member '" << primary->identifier << "'...\n";
                see_other_message(primary->location);
            } else {
                redeclaration_error(secondary, primary->location, "with no linkage");
            }
        }

        auto composite = composite_type(primary->type, secondary->type);
        if (composite) {
            primary->type = composite;
        } else {
            redeclaration_error(secondary, primary->location, "with incompatible type");
        }

        if (secondary_entity->initializer) {
            if (primary_entity->initializer) {
                redeclaration_error(secondary, primary->location, nullptr);
            } else {
                primary_entity->initializer = secondary_entity->initializer;
            }
        }
    
        if (secondary_entity->body) {
            if (primary_entity->body) {
                redeclaration_error(secondary, primary->location, nullptr);
            } else {
                primary_entity->body = secondary_entity->body;
                primary_entity->parameters = move(secondary_entity->parameters);
            }
        }
  
        primary_entity->inline_definition = secondary_entity->inline_definition && primary_entity->inline_definition;

        assert(secondary_entity->storage_duration() == primary_entity->storage_duration());

        return VisitDeclaratorOutput();
    }
    
    virtual VisitDeclaratorOutput visit(Declarator* primary, EnumConstant* primary_enum_constant, const VisitDeclaratorInput& input) override {
        auto secondary = input.secondary;
        if (!secondary) {
            resolve(primary_enum_constant->constant_expr);
            return VisitDeclaratorOutput();
        }

        auto secondary_enum_constant = secondary->enum_constant();
        assert(secondary_enum_constant); //  TODO

        if (!primary_enum_constant->enum_tag ||                                                       // enum { A }; enum E { A };
            !secondary_enum_constant->enum_tag ||                                                     // enum E { A }; enum { A };
            primary_enum_constant->enum_tag == secondary_enum_constant->enum_tag ||                   // enum E { A, A };
            primary_enum_constant->enum_tag->primary != secondary_enum_constant->enum_tag->primary    // enum E1 { A }; enum E2 { A };
        ) {
            redeclaration_error(secondary, primary->location, nullptr);
        }
        
        return VisitDeclaratorOutput();
    }

    bool compare_union_types(const UnionType* a_union, const UnionType* b_union) {
        if (!a_union->complete) return true;

        unordered_map<InternedString, Declarator*> a_members;
        for (auto a_member: a_union->members) {
            a_members.insert(make_pair(a_member->identifier.name, a_member));
        }

        for (auto b_member: b_union->members) {
            auto it = a_members.find(b_member->identifier.name);
            if (it == a_members.end()) return false;
            if (!compare_types(it->second->type, b_member->type)) return false;
        }

        return true;
    }

    bool compare_enum_types(const EnumType* a_enum, const EnumType* b_enum) {
        if (!a_enum->complete) return true;
        
        unordered_map<InternedString, Declarator*> a_constants;
        for (auto declarator: a_enum->constants) {
            a_constants.insert(make_pair(declarator->identifier.name, declarator));
        }

        for (auto b_declarator: b_enum->constants) {
            auto it = a_constants.find(b_declarator->identifier.name);
            if (it == a_constants.end()) {
                message(Severity::ERROR, b_declarator->location) << "enum constant '" << b_declarator->identifier << "'...\n";
                message(Severity::INFO, a_enum->location) << "...missing from other definition\n";
                pause_messages();
                return false;
            }

            auto a_declarator = it->second;
            auto b_enum_constant = b_declarator->enum_constant();
            auto a_enum_constant = a_declarator->enum_constant();
            if (b_enum_constant->constant_int != a_enum_constant->constant_int) {
                message(Severity::ERROR, b_declarator->location) << "incompatible enum constant '" << b_declarator->identifier << "' value " << b_enum_constant->constant_int << "...\n";
                message(Severity::INFO, a_declarator->location) << "...versus " << a_enum_constant->constant_int << " here\n";
                pause_messages();
                return false;
            }
        }

        return true;
    }

    bool compare_tags(const TagType* a_type, const TagType* b_type) {
        bool a_has_tag = a_type->tag;
        bool b_has_tag = b_type->tag;
        if (a_has_tag != b_has_tag) return false;
        
        if (a_has_tag && b_has_tag && a_type->tag->identifier.name != b_type->tag->identifier.name) return false;

        return true;
    }

    const Type* compare_types(const Type* a, const Type* b) {
        if (a == b) return a;

        if (typeid(*a) != typeid(*b)) return nullptr;

        if (auto a_tagged = dynamic_cast<const TagType*>(a)) {
            auto b_tagged = static_cast<const TagType*>(b);
            if (!compare_tags(a_tagged, b_tagged)) return nullptr;
        }

        if (auto a_struct = dynamic_cast<const StructType*>(a)) {
            auto b_struct = static_cast<const StructType*>(b);

            if (a_struct->complete && b_struct->complete) {
                for (size_t i = 0; i < a_struct->members.size(); ++i) {
                    auto a_declarator = a_struct->members[i];
                    auto b_declarator = b_struct->members[i];

                    if (a_declarator->identifier != b_declarator->identifier) return nullptr;
                    if (!compare_types(a_declarator->type, b_declarator->type)) return nullptr;

                    // TODO bitfield size, etc
                }
            }
        } else if (auto a_union = dynamic_cast<const UnionType*>(a)) {
            auto b_union = static_cast<const UnionType*>(b);
            if (!compare_union_types(a_union, b_union)) return nullptr;
            if (!compare_union_types(b_union, a_union)) return nullptr;
        } else if (auto a_enum = dynamic_cast<const EnumType*>(a)) {
            auto b_enum = static_cast<const EnumType*>(b);
            if (!compare_enum_types(a_enum, b_enum)) return nullptr;
            if (!compare_enum_types(b_enum, a_enum)) return nullptr;
        } else {
            return nullptr;
        }

        return (a->is_complete() || !b->is_complete()) ? a : b;
    }

    virtual VisitDeclaratorOutput visit(Declarator* primary, TypeDef* primary_type_def, const VisitDeclaratorInput& input) override {
        auto secondary = input.secondary;
        if (!secondary) {
            return VisitDeclaratorOutput();
        }

        auto type = compare_types(primary->type, secondary->type);
        if (!type) {
            redeclaration_error(secondary, primary->location, "with incompatible type");
            return VisitDeclaratorOutput();
        }

        assert(primary->type == type);

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
        // C99 6.7.2.3p4
        auto want_complete = type->complete;
        type->complete = false;

        for (auto member: type->members) {
            auto member_type = resolve(member);

            /*if (auto member_entity = member->entity()) {
                if (!member_type->is_complete()) {
                    message(Severity::ERROR, member->location) << "member '" << member->identifier << "' has incomplete type\n";
                }
            }*/
        }

        type->complete = want_complete;
        return VisitTypeOutput(type);
    }

    virtual VisitTypeOutput visit(const EnumType* type, const VisitTypeInput& input) override {
        // C99 6.7.2.3p4
        auto want_complete = type->complete;
        type->complete = false;

        type->base_type = IntegerType::default_type();
        long long next_int = 0;

        for (auto declarator: type->constants) {
            resolve(declarator);
            auto enum_constant = declarator->enum_constant();
            if (enum_constant->constant_expr) {
                auto value = fold_expr(enum_constant->constant_expr);
                next_int = LLVMConstIntGetSExtValue(value.llvm_const_rvalue());
            }

            enum_constant->constant_int = next_int;
            ++next_int;
        }

        type->complete = want_complete;
        return VisitTypeOutput(type);
    }

    virtual VisitTypeOutput visit(const TypeOfType* type, const VisitTypeInput& input) override {
        resolve(type->expr);
        return VisitTypeOutput(get_expr_type(type->expr));
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
            unsigned long long size_int = 1;
            auto size_constant = fold_expr(type->size, size_int);
            if (!size_constant.is_const_integer()) {
                message(Severity::ERROR, type->size->location) << "size of array must have integer type\n";
            } else {
                size_int = LLVMConstIntGetZExtValue(size_constant.llvm_const_rvalue());
            }

            return VisitTypeOutput(ResolvedArrayType::of(ArrayKind::COMPLETE, resolved_element_type, size_int));
        } else {
            return VisitTypeOutput(ResolvedArrayType::of(ArrayKind::INCOMPLETE, resolved_element_type, 0));
        }
    }

    VisitStatementOutput visit_default(Statement* statement, const VisitStatementInput& input) {
        assert(false);
        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(ForStatement* statement, const VisitStatementInput& input) override {
        if (statement->declaration) {
            for (auto declarator: statement->declaration->declarators) {
                resolve(declarator);
            }
        }

        resolve(statement->initialize);
        resolve(statement->condition);
        resolve(statement->iterate);

        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(GoToStatement* statement, const VisitStatementInput& input) override {
        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(IfElseStatement* statement, const VisitStatementInput& input) override {
        resolve(statement->condition);
        resolve(statement->then_statement);
        resolve(statement->else_statement);
        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(CompoundStatement* statement, const VisitStatementInput& input) override {
        resolve_pass(statement->scope, statement->nodes);
        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(ReturnStatement* statement, const VisitStatementInput& input) override {
        if (statement->expr) resolve(statement->expr);
        return VisitStatementOutput();
    }
    
    virtual VisitStatementOutput visit(SwitchStatement* statement, const VisitStatementInput& input) override {
        resolve(statement->expr);
        resolve(statement->body);

        for (auto case_expr: statement->cases) {
            resolve(case_expr);
        }

        return VisitStatementOutput();
    }
    
    virtual VisitStatementOutput visit_default(Expr* expr, const VisitStatementInput& input) override {
        assert(false);
        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(AddressExpr* address_expr, const VisitStatementInput& input) override {
        resolve(address_expr->expr);
        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(BinaryExpr* binary_expr, const VisitStatementInput& input) override {
        resolve(binary_expr->left);
        resolve(binary_expr->right);
        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(CallExpr* call_expr, const VisitStatementInput& input) override {
        resolve(call_expr->function);
        for (auto param: call_expr->parameters) {
            resolve(param);
        }
        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(CastExpr* cast_expr, const VisitStatementInput& input) override {
        cast_expr->type = resolve(cast_expr->type);
        resolve(cast_expr->expr);
        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(ConditionExpr* condition_expr, const VisitStatementInput& input) override {
        resolve(condition_expr->condition);
        resolve(condition_expr->then_expr);
        resolve(condition_expr->else_expr);
        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(DereferenceExpr* dereference_expr, const VisitStatementInput& input) override {
        resolve(dereference_expr->expr);
        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(EntityExpr* entity_expr, const VisitStatementInput& input) override {
        resolve(entity_expr->declarator);
        entity_expr->declarator = entity_expr->declarator->primary;
        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(IncDecExpr* expr, const VisitStatementInput& input) override {
        resolve(expr->expr);
        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(InitializerExpr* init_expr, const VisitStatementInput& input) override {
        for (auto element: init_expr->elements) {
            resolve(element);
        }
        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(SizeOfExpr* size_of_expr, const VisitStatementInput& input) override {
        size_of_expr->type = resolve(size_of_expr->type);
        if (!size_of_expr->type->is_complete()) {
            message(Severity::ERROR, size_of_expr->location) << "sizeof applied to incomplete type\n";
            pause_messages();
            return VisitStatementOutput();
        }

        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(SubscriptExpr* subscript_expr, const VisitStatementInput& input) override {
        resolve(subscript_expr->left);
        resolve(subscript_expr->right);
        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(FloatingPointConstant* constant, const VisitStatementInput& input) override {
        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(IntegerConstant* constant, const VisitStatementInput& input) override {
        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(StringConstant* constant, const VisitStatementInput& input) override {
        return VisitStatementOutput();
    }
};

void resolve_pass(const Scope& scope, const ASTNodeVector& nodes) {
    // Sort declarators so error messages don't vary between runs.
    vector<Declarator*> ordered;
    for (auto pair: scope.declarators) {
        ordered.push_back(pair.second);
    }

    sort(ordered.begin(), ordered.end(), [](Declarator* a, Declarator* b) {
        return a->location < b->location || (a->location == b->location && a < b);
    });

    ResolvePass pass;
    for (auto declarator: ordered) {
        resume_messages();
        pass.resolve(declarator);
    }
    
    for (auto node: nodes) {
        if (auto declaration = dynamic_cast<Declaration*>(node)) {
            pass.resolve(declaration->type);
        }

        if (auto statement = dynamic_cast<Statement*>(node)) {
            resume_messages();
            pass.resolve(statement);
        }
    }

    resume_messages();
}
