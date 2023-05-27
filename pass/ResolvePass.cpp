#include "Module.h"

#include "DepthFirstVisitor.h"
#include "Message.h"
#include "parse/Declaration.h"
#include "parse/ArrayType.h"
#include "parse/Expr.h"
#include "parse/IdentifierMap.h"
#include "parse/Type.h"
#include "TypeVisitor.h"

struct ResolvePass: DepthFirstVisitor, TypeVisitor {
    Module& result;
    unordered_set<const TagType*> tag_types;
    unordered_map<const Type*, const Type*> resolved_types;

    struct TodoLess {
        bool operator()(Declarator* a, Declarator* b) const {
            return a->location < b->location;
        }
    };

    set<Declarator*, TodoLess> todo;
    unordered_set<Declarator*> done;

    explicit ResolvePass(Module& module): result(module) {
    }

    void see_other_message(const Location& location) {
        message(Severity::INFO, location) << "... see other\n";
    }

    void redeclaration_message(Severity severity, const Declarator* secondary, const Location &primary_location, const char* problem) {
        auto& stream = message(severity, secondary->location);
        
        if (secondary->delegate->message_is_definition()) {
            stream << "redefinition";
        } else {
            stream << "redeclaration";
        }
        
        stream << " of " << secondary->message_kind() << " '" << *secondary->identifier << "'";
        
        if (problem) {
            stream << ' ' << problem;
        }

        stream << "\n";

        see_other_message(primary_location);
        pause_messages();
    }

    
    const Type* composite_type(const Type* a, const Type* b) {
        if (a == b) return a;

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

    void composite_entity(Declarator* primary, Entity* primary_entity, Declarator* secondary, Entity* secondary_entity) {
        if (primary_entity->linkage == Linkage::NONE || secondary_entity->linkage == Linkage::NONE) {
            if (primary->is_member()) {
                message(Severity::ERROR, secondary->location) << "duplicate member '" << *primary->identifier << "'\n";
                see_other_message(primary->location);
            } else {
                redeclaration_message(Severity::ERROR, secondary, primary->location, nullptr);
            }
        } else {
            if (primary_entity->linkage != secondary_entity->linkage) {
                primary_entity->linkage = secondary_entity->linkage = Linkage::INTERNAL;
            }
        }

        auto composite = composite_type(primary->type, secondary->type);
        if (composite) {
            primary->type = composite;
        } else {
            redeclaration_message(Severity::ERROR, secondary, primary->location, "with incompatible type");
        }
    }

    const Type* adjust_parameter_type(const Type* param_type) {
        if (auto array_type = dynamic_cast<const ArrayType*>(param_type->unqualified())) {
            // Instead of tranforming array type parameters to pointer type, pass arrays by reference.
            param_type = ReferenceType::of(param_type, ReferenceType::Kind::LVALUE, false);

            // C99 6.7.5.3p7
            // param_type = QualifiedType::of(array_type->element_type->pointer_to(), param_type->qualifiers());
        }

        // C99 6.7.5.3p8
        if (auto function_type = dynamic_cast<const FunctionType*>(param_type)) {
            param_type = param_type->pointer_to();
        }

        return param_type;
    }

    virtual VisitDeclaratorOutput visit(Declarator* primary, Variable* primary_variable, const VisitDeclaratorInput& input) override {
        auto secondary = input.secondary;
        if (!secondary) {
            auto output = Base::visit(primary, primary_variable, input);

            if (auto reference_type = dynamic_cast<const ReferenceType*>(primary->type)) {
                if (primary_variable->storage_duration != StorageDuration::AUTO) {
                    message(Severity::ERROR, primary->location) << primary->message_kind() << " '" << *primary->identifier
                                                                << "' cannot have reference type '" << PrintType(primary->type) << "'\n";
                    primary->type = reference_type->base_type;
                }
            }

            if (primary->scope && primary->scope->kind == ScopeKind::PROTOTYPE) {
                primary->type = adjust_parameter_type(primary->type);
            }

            if (primary_variable->initializer) {
                if (auto array_type = dynamic_cast<const ResolvedArrayType*>(primary->type)) {
                    auto initializer = primary_variable->initializer;

                    if (auto init_expr = dynamic_cast<InitializerExpr*>(initializer)) {
                        // C99 6.7.8p14,15
                        auto int_element_type = dynamic_cast<const IntegerType*>(array_type->element_type->unqualified());
                        if (int_element_type &&
                            init_expr->elements.size() == 1 &&
                            dynamic_cast<StringConstant*>(init_expr->elements[0]))
                        {
                            initializer = init_expr->elements[0];
                        } else {
                            // C99 6.7.8p22
                            if (auto resolved = compose_array_type_with_initializer_size(array_type, init_expr->elements.size())) {
                                primary->type = resolved;
                            } else if (array_type->kind == ArrayKind::COMPLETE && init_expr->elements.size() > array_type->size) {
                                message(Severity::ERROR, init_expr->elements[array_type->size]->location) << "excess elements in array initializer\n";
                            }
                        }
                    }

                    // C99 6.7.8p22
                    if (auto string_constant = dynamic_cast<StringConstant*>(initializer)) {
                        auto string_size = string_constant->value.length + 1;
                        if (auto resolved = compose_array_type_with_initializer_size(array_type, string_size)) {
                            primary->type = resolved;
                        } else if (array_type->kind == ArrayKind::COMPLETE && string_size > array_type->size) {
                            message(Severity::ERROR, string_constant->location) << "size of string literal (" << string_size << ") exceeds declared array size (" << array_type->size << ")\n";
                        }
                    }
                }
            }

            if (primary->type->partition() == TypePartition::INCOMPLETE) {
                message(Severity::ERROR, primary->location) << primary->message_kind() << " '" << *primary->identifier << "' has incomplete type\n";
                primary->type = IntegerType::default_type();
            }

            return output;
        }

        auto secondary_entity = secondary->variable();

        composite_entity(primary, primary_variable, secondary, secondary_entity);

        if (secondary_entity->initializer) {
            if (primary_variable->initializer) {
                // todo: allow this on globals if they evaluate to the same constant
                redeclaration_message(Severity::ERROR, secondary, primary->location, nullptr);
            } else {
                primary_variable->initializer = secondary_entity->initializer;
            }
        }

        assert(secondary_entity->storage_duration == primary_variable->storage_duration);

        return VisitDeclaratorOutput();
    }
    
    virtual VisitDeclaratorOutput visit(Declarator* primary, Function* primary_function, const VisitDeclaratorInput& input) override {
        auto secondary = input.secondary;
        if (!secondary) {
            auto function_type = dynamic_cast<const FunctionType*>(primary->type);
            for (size_t i = 0; i < primary_function->parameters.size(); ++i) {
                accept_declarator(primary_function->parameters[i]);
            }

            primary->status = DeclaratorStatus::RESOLVED;

            if (primary_function->body) {
                accept_statement(primary_function->body);
            }

            return VisitDeclaratorOutput();
        }

        auto secondary_entity = secondary->function();

        composite_entity(primary, primary_function, secondary, secondary_entity);
    
        if (secondary_entity->body) {
            if (primary_function->body) {
                redeclaration_message(Severity::ERROR, secondary, primary->location, nullptr);
            } else {
                primary_function->body = secondary_entity->body;
                primary_function->parameters = move(secondary_entity->parameters);
            }
        }
  
        primary_function->inline_definition = secondary_entity->inline_definition && primary_function->inline_definition;

        return VisitDeclaratorOutput();
    }

    virtual VisitDeclaratorOutput visit(Declarator* primary, EnumConstant* primary_enum_constant, const VisitDeclaratorInput& input) override {
        auto secondary = input.secondary;
        if (!secondary) {
            auto output = Base::visit(primary, primary_enum_constant, input);
            primary_enum_constant->type = dynamic_cast<const EnumType*>(resolve(primary_enum_constant->type));
            return output;
        }

        redeclaration_message(Severity::ERROR, secondary, primary->location, nullptr);
        
        return VisitDeclaratorOutput();
    }

    bool compare_tags(const TagType* a_type, const TagType* b_type) {
        bool a_has_tag = a_type->tag;
        bool b_has_tag = b_type->tag;
        if (a_has_tag != b_has_tag) return false;
        
        if (a_has_tag && b_has_tag && a_type->tag->identifier != b_type->tag->identifier) return false;

        return true;
    }

    const Type* compare_types(const Type* a, const Type* b) {
        if (a == b) return a;

        if (typeid(*a) != typeid(*b)) return nullptr;

        if (auto a_tagged = dynamic_cast<const TagType*>(a)) {
            auto b_tagged = static_cast<const TagType*>(b);
            if (!compare_tags(a_tagged, b_tagged)) return nullptr;

            auto a_partition = a->partition();
            auto b_partition = b->partition();

            if (a_partition == TypePartition::OBJECT && b_partition == TypePartition::OBJECT) return nullptr;

            return (a_partition == TypePartition::OBJECT || b_partition != TypePartition::OBJECT) ? a : b;
        }

        return nullptr;
    }

    virtual VisitDeclaratorOutput visit(Declarator* primary, TypeDelegate* primary_type_def, const VisitDeclaratorInput& input) override {
        auto secondary = input.secondary;
        if (!secondary) {
            if (auto reference_type = dynamic_cast<const ReferenceType*>(primary->type)) {
                message(Severity::ERROR, primary->location) << primary->message_kind() << " '" << *primary->identifier
                                                            << "' cannot have reference type '" << PrintType(primary->type) << "'\n";
                primary->type = reference_type->base_type;
            }

            return VisitDeclaratorOutput();
        }

        auto type = compare_types(primary->type, secondary->type);
        if (!type) {
            redeclaration_message(Severity::ERROR, secondary, primary->location, "with incompatible type");
            return VisitDeclaratorOutput();
        }

        assert(primary->type == type);

        return VisitDeclaratorOutput();
    }

    Declarator* lookup_member(const Type* enclosing_type, const Identifier& identifier, const Location& lookup_location) {
        if (auto tag_type = dynamic_cast<const TagType*>(enclosing_type)) {
            if (!tag_type->complete) {
                message(Severity::ERROR, lookup_location) << "incomplete type '" << PrintType(enclosing_type) << "' named in nested type specifier\n";
                message(Severity::INFO, tag_type->location) << "... see '" << PrintType(enclosing_type) << "'\n";
                pause_messages();
                return nullptr;
            }

            if (!tag_type->scope) {
                message(Severity::ERROR, lookup_location) << "no member named '" << identifier << "' in '" << PrintType(tag_type) << "' because it is unscoped\n";
                message(Severity::INFO, tag_type->location) << "... see '" << PrintType(enclosing_type) << "'\n";
                pause_messages();
                return nullptr;
            }

            auto member = tag_type->scope->lookup_member(identifier);
            if (!member) {
                message(Severity::ERROR, lookup_location) << "no member named '" << identifier << "' in '" << PrintType(tag_type) << "'\n";
                message(Severity::INFO, tag_type->location) << "... see '" << PrintType(enclosing_type) << "'\n";
                pause_messages();
                return nullptr;
            }

            accept_declarator(member);
            return member->primary;
        }

        message(Severity::ERROR, lookup_location) << "type '" << PrintType(enclosing_type) << "' may not contain member '" << identifier << "'\n";
        pause_messages();

        return nullptr;
    }

    virtual const Type* visit(const NestedType* type) override {
        auto enclosing_type = resolve(type->enclosing_type);

        auto member = lookup_member(enclosing_type, type->identifier, type->location);
        if (!member) return enclosing_type;

        if (!member->type_delegate()) {
            message(Severity::ERROR, type->location) << "member '" << type->identifier << "' is not a nested type\n";
            if (auto tag_type = dynamic_cast<const TagType*>(enclosing_type)) {
                message(Severity::INFO, tag_type->location) << "... see '" << PrintType(enclosing_type) << "'\n";
            }
        }

        return member->type;
    }

    virtual const Type* visit(const PointerType* type) override {
        return resolve(type->base_type)->pointer_to();
    }

    virtual const Type* visit(const ReferenceType* type) override {
        return ReferenceType::of(resolve(type->base_type), type->kind, type->captured);
    }

    virtual const Type* visit(const QualifiedType* type) override {
        return QualifiedType::of(resolve(type->base_type), type->qualifier_flags);
    }

    virtual const Type* visit(const UnqualifiedType* type) override {
        return resolve(type->base_type)->unqualified();
    }

    virtual const Type* visit(const FunctionType* type) override {
        auto resolved_return_type = resolve(type->return_type);
        auto resolved_param_types(type->parameter_types);
        for (auto& param_type : resolved_param_types) {
            param_type = adjust_parameter_type(resolve(param_type));
        }
        return FunctionType::of(resolved_return_type, resolved_param_types, type->variadic);
    }

    virtual const Type* visit(const StructType* type) override {
        return visit_structured_type(type);
    }

    virtual const Type* visit(const UnionType* type) override {
        return visit_structured_type(type);
    }

    const Type* visit_structured_type(const StructuredType* type) {
        pend(type->tag);

        if (!type->complete) return type;

        if (type->scope) {
            result.type_scopes.push_back(type->scope);        
        }

        // C99 6.7.2.3p4
        bool want_complete = type->complete;
        type->complete = false;

        SCOPE_EXIT {
            type->complete = want_complete;
        };

        for (auto declaration: type->declarations) {
            declaration->type = resolve(declaration->type);

            for (auto declarator: declaration->declarators) {
                if (auto variable = declarator->variable()) {
                    accept_declarator(declarator);
                } else {
                    pend(declarator);
                }
            }
        }

        return type;
    }

    virtual const Type* visit(const EnumType* type) override {
        pend(type->tag);

        if (!type->complete) return type;

        // C99 6.7.2.3p4
        bool want_complete = type->complete;
        type->complete = false;

        for (auto declarator: type->constants) {
            declarator->type = type->base_type;
        }

        SCOPE_EXIT {
            type->complete = want_complete;

            for (auto declarator: type->constants) {
                declarator->type = type;
            }
        };

        long long next = 0;
        for (auto declarator: type->constants) {
            auto enum_constant = declarator->enum_constant();
            if (enum_constant->expr) {
                enum_constant->expr = accept_expr(enum_constant->expr).expr;
                auto value = fold_expr(enum_constant->expr);
                if (value.is_const_integer()) {
                    next = LLVMConstIntGetSExtValue(value.get_const());                
                } else if (value.is_valid()) {
                    message(Severity::ERROR, enum_constant->expr->location) << "enum constant type '" << PrintType(value.type) << "' is not an integer type\n";
                }
            }

            enum_constant->value = next++;
            enum_constant->ready = true;

            pend(declarator);
        }

        type->base_type = resolve(type->base_type);
        if (!dynamic_cast<const IntegerType*>(type->base_type)) {
            message(Severity::ERROR, type->location) << "type '" << PrintType(type->base_type) << "' is not a valid integer enum base type\n";
            type->base_type = IntegerType::default_type();
            type->explicit_base_type = false;
        }

        return type;
    }

    virtual const Type* visit(const TypeOfType* type) override {
        auto expr = accept_expr(type->expr).expr;
        return get_expr_type(expr);
    }

    virtual const Type* visit(const TypeDefType* type) override {
        accept_declarator(type->declarator);
        return type->declarator->primary->type;
    }

    virtual const Type* visit(const UnresolvedArrayType* type) override {
        auto resolved_element_type = resolve(type->element_type);
        if (resolved_element_type->partition() != TypePartition::OBJECT) {
            if (resolved_element_type->partition() == TypePartition::INCOMPLETE) {
                message(Severity::ERROR, type->location) << "array element has incomplete type\n";
            } else {
                message(Severity::ERROR, type->location) << "array element type may not be function\n";
            }
            resolved_element_type = IntegerType::default_type();
        }

        Expr* size = type->size;
        if (size) {
            size = accept_expr(size).expr;
            unsigned long long size_int = 1;
            auto size_constant = fold_expr(size);
            if (!size_constant.is_const_integer()) {
                message(Severity::ERROR, type->size->location) << "size of array must have integer type\n";
            } else {
                size_int = LLVMConstIntGetZExtValue(size_constant.get_const());
            }

            return ResolvedArrayType::of(ArrayKind::COMPLETE, resolved_element_type, size_int);
        } else {
            return ResolvedArrayType::of(ArrayKind::INCOMPLETE, resolved_element_type, 0);
        }
    }

    virtual VisitStatementOutput visit(CompoundStatement* statement) override {
        auto output = Base::visit(statement);

        for (auto node: statement->nodes) {
            if (auto declaration = dynamic_cast<Declaration*>(node)) {
                declaration->type = resolve(declaration->type);
            }
        }

        return output;
    }

    virtual VisitExpressionOutput visit(CastExpr* expr) override {
        auto output = Base::visit(expr);
        expr->type = resolve(expr->type);
        return output;
    }


    virtual VisitExpressionOutput visit(EntityExpr* expr) override {
        auto output = Base::visit(expr);

        expr->declarator = expr->scope->lookup_declarator(expr->identifier);
        if (!expr->declarator) {
            auto& stream = message(Severity::ERROR, expr->location) << "identifier '" << *expr->identifier.text << "' ";
            if (expr->identifier.text != expr->identifier.usage_at_file_scope) {
                stream << "(aka '" << *expr->identifier.usage_at_file_scope << "') ";
            }
            stream << "undeclared\n";
            return Base::visit(IntegerConstant::default_expr(expr->location));
        }

        accept_declarator(expr->declarator);
        expr->declarator = expr->declarator->primary;

        return output;
    }

    // If the LHS of a MemberExpr is a type, return that resolved type, else null.
    const Type* wrangle_member_expr_enclosing_type(MemberExpr* expr) {
        if (expr->type) {
            return resolve(expr->type);
        }

        if (auto entity = dynamic_cast<EntityExpr*>(expr->object)) {
            if (!entity->declarator->type_delegate()) return nullptr;

            accept_declarator(entity->declarator);
            return entity->declarator->primary->type;
        }

        if (auto nested_member = dynamic_cast<MemberExpr*>(expr->object)) {
            auto enclosing_type = wrangle_member_expr_enclosing_type(nested_member);
            if (!enclosing_type) return nullptr;

            auto member = lookup_member(enclosing_type, nested_member->identifier, nested_member->location);
            if (!member) return nullptr;
            
            return member->primary->type;
        }

        return nullptr;
    }

    virtual VisitExpressionOutput visit(MemberExpr* member_expr) override {
        auto output = Base::visit(member_expr);

        auto enclosing_type = wrangle_member_expr_enclosing_type(member_expr);
        if (enclosing_type) {
            member_expr->type = enclosing_type;
            member_expr->object = nullptr;
        } else {
            enclosing_type = get_expr_type(member_expr->object);
            enclosing_type = resolve(enclosing_type);
        }

        enclosing_type = enclosing_type->unqualified();
        auto message_type = enclosing_type;

        bool dereferenced{};
        if (auto pointer_type = dynamic_cast<const PointerType*>(enclosing_type)) {
            dereferenced = true;
            enclosing_type = pointer_type->base_type->unqualified();
        }

        if (auto tagged_type = dynamic_cast<const TagType*>(enclosing_type)) {
            auto member = lookup_member(enclosing_type, member_expr->identifier, member_expr->location);
            if (!member) return VisitExpressionOutput(IntegerConstant::default_expr(member_expr->location));

            if (dereferenced) {
                if (member_expr->op == '.') {
                    message(Severity::ERROR, member_expr->location) << "type '" << PrintType(message_type) << "' is a pointer; consider using the '->' operator instead of '.'\n";
                }
            } else {
                if (member_expr->op == TOK_PTR_OP) {
                    message(Severity::ERROR, member_expr->location) << "type '" << PrintType(message_type) << "' is not a pointer; consider using the '.' operator instead of '->'\n";
                }
            }

            member_expr->member = member->primary;
            return output;
        }

        Location location = member_expr->object ? member_expr->object->location : member_expr->location;
        message(Severity::ERROR, location) << "type '" << PrintType(enclosing_type) << "' does not have members\n";
        pause_messages();
        return output;
    }

    virtual VisitExpressionOutput visit(SizeOfExpr* expr) override {
        auto output = Base::visit(expr);
        
        expr->type = resolve(expr->type);
        if (expr->type->partition() == TypePartition::INCOMPLETE) {
            message(Severity::ERROR, expr->location) << "sizeof applied to incomplete type\n";
            pause_messages();
            return Base::visit(expr);
        }

        return output;
    }

    bool is_trivially_cyclic(Declarator* primary, const Type* type) {
        for (;;) {
            if (auto qt = dynamic_cast<const QualifiedType*>(type)) {
                type = qt->base_type;
            } else if (auto tdt = dynamic_cast<const TypeDefType*>(type)) {
                return tdt->declarator->primary == primary;
            } else {
                return false;
            }
        }
    }

    void fix_misidentified_function(Declarator* declarator) {
        auto variable = declarator->variable();
        if (!variable) return;

        if (variable->initializer) return;

        if (variable->member) {
            if (variable->member->bit_field) return;
        }

        declarator->delegate = new Function(variable->linkage);
    }

    virtual VisitDeclaratorOutput accept_declarator(Declarator* primary) override {
        struct ResolutionCycle {};

        primary = primary->primary;
        if (primary->status >= DeclaratorStatus::RESOLVED) return VisitDeclaratorOutput();
        if (primary->status == DeclaratorStatus::RESOLVING) {
            throw ResolutionCycle();
        }

        primary->status = DeclaratorStatus::RESOLVING;

        if (!primary->type) {
            message(Severity::ERROR, primary->location) << "declaration directive not matched with a proper declaration of '" << *primary->identifier << "'\n";
            primary->type = IntegerType::default_type();
            primary->delegate = new Variable(Linkage::NONE, StorageDuration::STATIC);
        }

        Declarator* acyclic_declarator{};
        for (auto declarator = primary; declarator; declarator = declarator->next) {
            try {
                if (declarator->type->has_tag(declarator) && declarator->type->partition() != TypePartition::INCOMPLETE) {
                    swap(declarator->type, primary->type);
                    acyclic_declarator = primary;
                    primary->status = DeclaratorStatus::RESOLVED;
                    auto resolved_type = resolve(primary->type);
                    assert(resolved_type == primary->type);  // must be because declarator was already marked resolved
                    break;
                }

                declarator->type = resolve(declarator->type);
                if (!acyclic_declarator || (declarator->type->partition() != TypePartition::INCOMPLETE && acyclic_declarator->type->partition() == TypePartition::INCOMPLETE)) {
                    acyclic_declarator = declarator;
                }
            } catch (ResolutionCycle) {
                if (!is_trivially_cyclic(primary, declarator->type)) {
                    message(Severity::ERROR, declarator->location) << "recursive definition of '" << *declarator->identifier << "'\n";
                    pause_messages();
                }
            }
        }

        if (primary->type_delegate()) primary->status = DeclaratorStatus::RESOLVED;

        if (acyclic_declarator) {
            swap(acyclic_declarator->type, primary->type);
            assert(!dynamic_cast<const TypeDefType*>(primary->type));

            bool is_function = dynamic_cast<const FunctionType*>(primary->type);
            if (is_function) fix_misidentified_function(primary);

            for (auto secondary = primary->next; secondary; secondary = secondary->next) {
                if (is_function) fix_misidentified_function(secondary);

                try {
                    secondary->type = resolve(secondary->type);
                } catch (ResolutionCycle) {
                    message(Severity::ERROR, secondary->location) << "recursive definition of '" << *secondary->identifier << "'\n";
                    pause_messages();
                }

                if (typeid(*secondary->delegate) != typeid(*primary->delegate)) {
                    redeclaration_message(Severity::ERROR, secondary, primary->location, "with different kind of identifier");
                } else {
                    primary->accept(*this, VisitDeclaratorInput(secondary));
                }
            }
        } else {
            auto declarator = primary;
            while (declarator->next) {
                declarator = declarator->next;
            }
            primary->type = IntegerType::default_type();
            message(Severity::ERROR, declarator->location) << "'" << *declarator->identifier << "' undeclared\n";
        }

        primary->next = nullptr;
        primary->accept(*this, VisitDeclaratorInput());
        primary->status = DeclaratorStatus::RESOLVED;

        return VisitDeclaratorOutput();
    }

    const Type* resolve(const Type* unresolved_type) {
        auto& resolved_type = resolved_types[unresolved_type];
        if (resolved_type) return resolved_type;

        return resolved_type = unresolved_type->accept(*this);
    }
    
    void resolve(const vector<Declaration*>& declarations) {
        for (auto declaration: declarations) {
            declaration->type = resolve(declaration->type);
            todo.insert(declaration->declarators.begin(), declaration->declarators.end());
        }

        while (!todo.empty()) {
            auto node = *todo.begin();
            todo.erase(todo.begin());
            done.insert(node);

            resume_messages();
            accept_declarator(node);
        }
    
        resume_messages();

        for (auto scope: result.type_scopes) {
            scope->type->llvm_type();
        }

        sort(result.type_scopes.begin(), result.type_scopes.end(), [](const Scope* a, const Scope* b) {
            return a->type->location < b->type->location;
        });
    }

    void pend(Declarator* declarator) {
        if (!declarator) return;
        if (done.find(declarator) != done.end()) return;
        todo.insert(declarator);
    }
};

void Module::resolve_pass(const vector<Declaration*>& declarations, Scope& file_scope) {
    ResolvePass pass(*this);
    pass.result.file_scope = &file_scope;
    pass.resolve(declarations);
}
