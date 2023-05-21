#include "Module.h"
#include "lex/StringLiteral.h"
#include "LLVM.h"
#include "Message.h"
#include "parse/AssocPrec.h"
#include "parse/Declaration.h"
#include "TranslationUnitContext.h"
#include "TypeVisitor.h"
#include "ValueWrangler.h"
#include "Visitor.h"

struct Emitter;

struct PendingDestructor {
    Value addressible_value;
};

struct EmitterScope {
    EmitterScope* parent_scope{};
    Emitter* emitter{};

    vector<PendingDestructor> destructors;

    EmitterScope(Emitter* emitter);
    virtual ~EmitterScope();
};

struct Construct: EmitterScope {
    Construct* parent_construct{};
    LLVMBasicBlockRef continue_block{};
    LLVMBasicBlockRef break_block{};

    Construct(Emitter* emitter);
};

struct SwitchConstruct: Construct {
    SwitchConstruct* parent_switch{};

    unordered_map<Expr*, LLVMBasicBlockRef> case_labels;
    LLVMBasicBlockRef default_label{};

    SwitchConstruct(Emitter* emitter);
    ~SwitchConstruct();
};

struct Emitter: ValueWrangler, Visitor {
    const EmitOptions& options;

    Emitter* parent{};

    Declarator* function_declarator{};
    const FunctionType* function_type{};
    LLVMValueRef function{};
    LLVMBasicBlockRef unreachable_block{};
    unordered_map<InternedString, LLVMBasicBlockRef> goto_labels;
    EmitterScope* innermost_scope{};
    SwitchConstruct* innermost_switch{};
    InternedString this_string;
    Value this_value;
    LLVMTypeRef destructor_wrapper_type{};
    unordered_map<const StructuredType*, TypedFunctionRef> destructor_wrappers;

    Emitter(Module* module, EmitOutcome outcome, const EmitOptions& options)
        : ValueWrangler(module, outcome), options(options) {
        this_string = intern_string("this");
    }
    
    TypedFunctionRef destructor_wrapper(const StructuredType* type) {
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
        ref.function = LLVMAddFunction(module->llvm_module, "destructor_wrapper", ref.type);
        destructor_wrappers[type] = ref;

        if (!options.emit_helpers) return ref;

        LLVMSetLinkage(ref.function, LLVMInternalLinkage);

        auto entry_block = LLVMAppendBasicBlockInContext(context->llvm_context, ref.function, "");
        LLVMPositionBuilderAtEnd(builder, entry_block);

        auto receiver = LLVMGetParam(ref.function, 0);
        auto llvm_type = type->llvm_type();
        auto current_state = LLVMBuildLoad2(builder, llvm_type, receiver, "");

        auto indeterminate = module->indeterminate_bool();
        auto selected = LLVMBuildSelect(builder, indeterminate.dangerously_get_value(builder, outcome),
                                                 current_state,
                                                 LLVMConstNull(llvm_type), "");

        auto is_const = call_is_constant_intrinsic(Value(type, selected));

        auto destructor_block = LLVMAppendBasicBlockInContext(context->llvm_context, ref.function, "");
        auto skip_block = LLVMAppendBasicBlockInContext(context->llvm_context, ref.function, "");

        LLVMBuildCondBr(builder, is_const.dangerously_get_value(builder, outcome), skip_block, destructor_block);

        LLVMPositionBuilderAtEnd(builder, destructor_block);
        LLVMBuildCall2(builder, type->destructor->type->llvm_type(), get_address(function->value), &receiver, 1, "");
        LLVMBuildBr(builder, skip_block);

        LLVMPositionBuilderAtEnd(builder, skip_block);
        LLVMBuildRetVoid(builder);

        LLVMPositionBuilderAtEnd(builder, old_block);

        return ref;
    }

    bool call_destructor_immediately(const Value& value) {
        auto context = TranslationUnitContext::it;

        if (!value.has_address) return false;

        auto structured_type = unqualified_type_cast<StructuredType>(value.type->unqualified());
        if (!structured_type) return false;

        auto destructor = structured_type->destructor;
        if (!destructor) return false;

        auto wrapper = destructor_wrapper(structured_type);
        auto arg = value.dangerously_get_address();
        LLVMBuildCall2(builder, wrapper.type, wrapper.function, &arg, 1, "");

        return true;
    }

    void call_pending_destructors(const EmitterScope& scope) {
        auto& destructors = scope.destructors;
        for (auto it = destructors.rbegin(); it != destructors.rend(); ++it) {
            call_destructor_immediately(it->addressible_value);
        }
    }

    void call_pending_destructors_at_all_scopes() {
        for (auto scope = innermost_scope; scope; scope = scope->parent_scope) {
            call_pending_destructors(*scope);
        }
    }

    bool pend_destructor(Value& value) {
        auto structured_type = unqualified_type_cast<StructuredType>(value.type->unqualified());
        if (!structured_type) return false;

        if (!structured_type->destructor) return false;

        make_addressable(value);

        innermost_scope->destructors.push_back(PendingDestructor(value));
        return true;
    }

    virtual VisitDeclaratorOutput accept_declarator(Declarator* declarator) override {
        declarator = declarator->primary;
        if (declarator->status >= DeclaratorStatus::EMITTED) return VisitDeclaratorOutput();
        assert(declarator->status == DeclaratorStatus::RESOLVED);

        declarator->accept(*this, VisitDeclaratorInput());

        declarator->status = DeclaratorStatus::EMITTED;

        return VisitDeclaratorOutput();
    }

    LLVMBasicBlockRef append_block(const char* name) {
        auto llvm_context = TranslationUnitContext::it->llvm_context;
        return LLVMAppendBasicBlockInContext(llvm_context, function, name);
    }

    LLVMBasicBlockRef lookup_label(const Identifier& identifier) {
        auto& block = goto_labels[identifier.text];
        if (!block) {
            block = append_block(identifier.c_str());
        }
        return block;
    }

    // Destruction Of Temporaries
    // 
    // By default, emit_expr pends a destructor call whenever it encounters an expression that evaluated to
    // an rvalue with a destructor. There are two cases when this is not the right thing to do.
    
    // The first is case is where a value "passes through" an expression, such as the conditional expression,
    // and emit_expr must not pend a second destructor call for the same object. In this case, pass false to
    // pend_temporary_destructor.
    //
    // The second case is where there is an lvalue where the result could be stored if they have the same
    // unqualified type. In this case, use store_and_pend_destructor, which will store the result directly in
    // the lvalue if possible or otherwise convert and pend a destructor call for the temporary.

    Value emit_expr(Expr* expr, bool pend_temporary_destructor = true) {
        try {
            auto output =  expr->accept(*this);

            if (pend_temporary_destructor && outcome == EmitOutcome::IR && output.value.kind == ValueKind::RVALUE) {
                pend_destructor(output.value);
            }

            return output.value;
        } catch (FoldError& e) {
            if (!e.error_reported) {
                message(Severity::ERROR, expr->location) << "not a constant expression\n";
            }
            throw FoldError(true);
        }
    }

    void store_and_pend_destructor(const Value& dest, Value source, const Location& dest_location, const Location& source_location) {
        if (dest.type->unqualified() == source.type->unqualified()) {
            // Destructor was already pended for the dest lvalue
            store(dest, get_value(source, source_location, false), dest_location);
        } else {
            source = convert_to_type(source, dest.type, ConvKind::IMPLICIT, source_location);
            pend_destructor(source);
            store(dest, get_value(source, source_location, false), dest_location);
        }
    }

    Value emit_full_expr(Expr* expr) {
        EmitterScope scope(this);
        auto value = emit_expr(expr);
        return value;
    }

    virtual VisitStatementOutput accept_statement(Statement* statement) override {
        if (!statement) return VisitStatementOutput();

        for (auto& label: statement->labels) {
            LLVMBasicBlockRef labelled_block{};
            if (label.kind == LabelKind::GOTO) {
                labelled_block = lookup_label(label.identifier);
            } else if (label.kind == LabelKind::CASE) {
                labelled_block = innermost_switch->case_labels[label.case_expr];
            } else if (label.kind == LabelKind::DEFAULT) {
                labelled_block = innermost_switch->default_label;
            }

            auto current_block = LLVMGetInsertBlock(builder);
            LLVMBuildBr(builder, labelled_block);
            LLVMMoveBasicBlockAfter(labelled_block, current_block);
            LLVMPositionBuilderAtEnd(builder, labelled_block);
        }

        statement->accept(*this);

        return VisitStatementOutput();
    }

    Value convert_to_type(const Value& value, const Type* dest_type, ConvKind kind, const Location& location) {
        return ValueWrangler::convert_to_type(value, dest_type, kind, location);
    }

    LLVMValueRef convert_to_rvalue(const Value& value, const Type* dest_type, ConvKind kind, const Location& location) {
        return get_value(convert_to_type(value, dest_type, kind, location), location);
    }

    Value convert_to_type(Expr* expr, const Type* dest_type, ConvKind kind) {
        auto value = emit_expr(expr).unqualified();
        return convert_to_type(value, dest_type, kind, expr->location);
    }

    LLVMValueRef convert_to_rvalue(Expr* expr, const Type* dest_type, ConvKind kind) {
        return get_value(convert_to_type(expr, dest_type, kind), expr->location);
    }

    void emit_function_definition(Declarator* declarator, Function* entity) {
        function_declarator = declarator;
        function_type = unqualified_type_cast<FunctionType>(declarator->primary->type);
        function = get_address(entity->value);

        entry_block = append_block("");
        unreachable_block = append_block("");
        LLVMPositionBuilderAtEnd(builder, entry_block);

        {
            EmitterScope scope(this);

            this_value = Value();
            for (size_t i = 0; i < entity->parameters.size(); ++i) {
                auto param = entity->parameters[i];
                auto param_entity = param->entity();

                auto llvm_param = LLVMGetParam(function, i);

                if (auto reference_type = dynamic_cast<const PassByReferenceType*>(param->type)) {
                    param_entity->value = Value(ValueKind::LVALUE, reference_type->base_type, llvm_param);

                    if (reference_type->kind == PassByReferenceType::Kind::RVALUE) {
                        pend_destructor(param_entity->value);
                    }
                } else {
                    auto storage = LLVMBuildAlloca(builder, param->type->llvm_type(), c_str(param->identifier));
                    param_entity->value = Value(ValueKind::LVALUE, param->type, storage);
                    LLVMBuildStore(builder, llvm_param, storage);

                    pend_destructor(param_entity->value);
                }

                if (param->identifier == this_string) {
                    this_value = param_entity->value;
                }
            }

            accept_statement(entity->body);
        }

        if (function_type->return_type->unqualified() == &VoidType::it) {
            LLVMBuildRetVoid(builder);
        } else {
            LLVMBuildRet(builder, LLVMConstNull(function_type->return_type->llvm_type()));
        }
        LLVMDeleteBasicBlock(unreachable_block);

        function = nullptr;
    }

    Value emit_scalar_initializer(const Type* dest_type, InitializerExpr* initializer) {
        if (initializer->elements.empty()) {
            // C23 6.7.10p12
            return Value(dest_type, LLVMConstNull(dest_type->llvm_type()));
        } else {
            if (initializer->elements.size() != 1) {
                message(Severity::ERROR, initializer->elements[1]->location) << "excess elements in scalar initializer\n";
            }

            // C99 6.7.8p11
            return convert_to_type(initializer->elements[0], dest_type, ConvKind::IMPLICIT);
        }
    }

    void emit_auto_initializer(const Value& dest, Expr* expr) {
        auto context = TranslationUnitContext::it;

        if (auto uninitializer = dynamic_cast<UninitializedExpr*>(expr)) {
            return;
        }

        Value scalar_value;
        if (auto initializer = dynamic_cast<InitializerExpr*>(expr)) {
            if (auto array_type = unqualified_type_cast<ResolvedArrayType>(dest.type)) {
                for (size_t i = 0; i < array_type->size; ++i) {
                    LLVMValueRef indices[] = { context->zero_size, Value::of_size(i).get_const() };
                    LLVMValueRef dest_element = LLVMBuildGEP2(builder, array_type->llvm_type(), get_address(dest), indices, 2, "");
                    emit_auto_initializer(Value(ValueKind::LVALUE, array_type->element_type, dest_element), initializer->elements[i]);
                }
                return;
            }

            if (auto struct_type = unqualified_type_cast<StructType>(dest.type)) {
                size_t initializer_idx{};
                for (auto declaration: struct_type->declarations) {
                    for (auto member: declaration->declarators) {
                        if (auto member_variable = member->variable()) {
                            LLVMValueRef dest_element = LLVMBuildInBoundsGEP2(builder, struct_type->llvm_type(), get_address(dest),
                                                                              member_variable->member->gep_indices.data(), member_variable->member->gep_indices.size(),
                                                                              c_str(member->identifier));
                            emit_auto_initializer(Value(ValueKind::LVALUE, member->type, dest_element), initializer->elements[initializer_idx++]);
                        }
                    }
                }

                return;
            }

            scalar_value = emit_scalar_initializer(dest.type, initializer);
        } else {
            auto source = emit_expr(expr, false);
            store_and_pend_destructor(dest, source, expr->location, expr->location);
            return;
        }

        LLVMBuildStore(builder, get_value(scalar_value, expr->location), get_address(dest));
    }

    LLVMValueRef emit_static_initializer(const Type* dest_type, Expr* expr) {
        if (auto uninitializer = dynamic_cast<UninitializedExpr*>(expr)) {
            return LLVMGetUndef(dest_type->llvm_type());
        }

        if (auto initializer = dynamic_cast<InitializerExpr*>(expr)) {
            if (auto array_type = unqualified_type_cast<ResolvedArrayType>(dest_type)) {
                vector<LLVMValueRef> values(array_type->size);
                for (size_t i = 0; i < array_type->size; ++i) {
                    values[i] = emit_static_initializer(array_type->element_type, initializer->elements[i]);
                }

                return LLVMConstArray(array_type->element_type->llvm_type(), values.data(), values.size());
            }

            if (auto struct_type = unqualified_type_cast<StructType>(dest_type)) {
                size_t initializer_idx{};
                vector<LLVMValueRef> values;
                for (auto declaration: struct_type->declarations) {
                    for (auto member: declaration->declarators) {
                        if (auto member_variable = member->variable()) {
                            values.push_back(emit_static_initializer(member->type, initializer->elements[initializer_idx++]));
                        }
                    }
                }

                return LLVMConstNamedStruct(struct_type->llvm_type(), values.data(), values.size());
            }

            return get_value(emit_scalar_initializer(dest_type, initializer), initializer->location);
        }

        return convert_to_rvalue(expr, dest_type, ConvKind::IMPLICIT);
    }

    void emit_variable(Declarator* declarator, Variable* entity) {
        auto type = declarator->primary->type;
        auto llvm_type = type->llvm_type();

        if (entity->storage_duration == StorageDuration::AUTO) {
            entity->value = allocate_auto_storage(type, c_str(declarator->identifier));

            bool has_destructor = pend_destructor(entity->value);

            if (entity->initializer) {
                EmitterScope scope(this);
                emit_auto_initializer(entity->value, entity->initializer);
            } else if (options.initialize_variables || has_destructor) {
                store(entity->value, Value::of_null(type).get_const(), declarator->location);
            }

        } else if (entity->storage_duration == StorageDuration::STATIC) {
            auto global = get_address(entity->value);

            LLVMValueRef initial = LLVMConstNull(llvm_type);
            if (entity->initializer) {
                Emitter initializer_emitter(module, EmitOutcome::FOLD, options);

                Value value;
                try {
                    initial = initializer_emitter.emit_static_initializer(type, entity->initializer);
                } catch (FoldError& e) {
                    if (!e.error_reported) {
                        message(Severity::ERROR, entity->initializer->location) << "static initializer is not a constant expression\n";
                    }
                }
            }
            LLVMSetInitializer(global, initial);

            entity->value = Value(ValueKind::LVALUE, type, global);
        }
    }

    virtual VisitDeclaratorOutput visit(Declarator* declarator, Variable* variable, const VisitDeclaratorInput& input) override {
        assert(declarator == declarator->primary);
          
        emit_variable(declarator, variable);

        return VisitDeclaratorOutput();
    }

    virtual VisitDeclaratorOutput visit(Declarator* declarator, Function* function, const VisitDeclaratorInput& input) override {
        assert(declarator == declarator->primary);
          
        if (function->body) {
            Emitter function_emitter(module, EmitOutcome::IR, options);
            function_emitter.parent = this;
            function_emitter.emit_function_definition(declarator, function);
        }

        return VisitDeclaratorOutput();
    }

    VisitStatementOutput visit(CompoundStatement* statement) {
        EmitterScope scope(this);

        for (auto node: statement->nodes) {
            if (auto declaration = dynamic_cast<Declaration*>(node)) {
                for (auto declarator: declaration->declarators) {
                    accept_declarator(declarator);
                }
            }

            if (auto statement = dynamic_cast<Statement*>(node)) {
                accept_statement(statement);
            }
        }

        return VisitStatementOutput();
    }

    VisitStatementOutput visit(ExprStatement* statement) {
        emit_full_expr(statement->expr);
        return VisitStatementOutput();
    }

    VisitStatementOutput visit(ForStatement* statement) {
        Construct construct(this);

        if (statement->declaration) {
            for (auto declarator: statement->declaration->declarators) {
                accept_declarator(declarator);
            }
        }

        if (statement->initialize) {
            emit_full_expr(statement->initialize);
        }

        auto loop_block = append_block("for_l");
        auto body_block = statement->condition ? append_block("for_b") : loop_block;
        auto iterate_block = statement->iterate ? construct.continue_block = append_block("for_i") : nullptr;
        auto end_block = construct.break_block = append_block("for_e");
        LLVMBuildBr(builder, loop_block);
        LLVMPositionBuilderAtEnd(builder, loop_block);

        bool synthesize_side_effect = true;
        if (statement->condition) {
            auto condition_value = convert_to_rvalue(statement->condition, IntegerType::of_bool(), ConvKind::IMPLICIT);
            LLVMBuildCondBr(builder, condition_value, body_block, end_block);

            synthesize_side_effect = LLVMIsAConstant(condition_value);
        }

        LLVMPositionBuilderAtEnd(builder, body_block);

        // C11 6.8.5p6
        if (synthesize_side_effect) {
            call_sideeffect_intrinsic();
        }

        accept_statement(statement->body);

        if (statement->iterate) {
            LLVMBuildBr(builder, iterate_block);

            LLVMPositionBuilderAtEnd(builder, iterate_block);
            emit_full_expr(statement->iterate);
        }

        LLVMBuildBr(builder, loop_block);
        LLVMPositionBuilderAtEnd(builder, end_block);

        return VisitStatementOutput();
    }

    VisitStatementOutput visit(GoToStatement* statement) {
        LLVMBasicBlockRef target_block{};
        if (statement->kind == TOK_GOTO) {
            // todo: consider what to do here in terms of calling destructors
            target_block = lookup_label(statement->identifier);
        } else {
            for (auto scope = innermost_scope; scope; scope = scope->parent_scope) {
                call_pending_destructors(*scope);

                if (auto construct = dynamic_cast<Construct*>(scope)) {
                    switch (statement->kind) {
                      case TOK_BREAK:
                        target_block = construct->break_block;
                        break;
                      case TOK_CONTINUE:
                        target_block = construct->continue_block;
                        break;
                    }
                }
            }
        }

        if (target_block) {
            LLVMBuildBr(builder, target_block);
            LLVMPositionBuilderAtEnd(builder, unreachable_block);
        } else {
            message(Severity::ERROR, statement->location) << "'" << statement->message_kind() << "' statement not in loop or switch statement\n";
        }

        return VisitStatementOutput();
    }

    VisitStatementOutput visit(IfElseStatement* statement) {
        auto then_block = append_block("if_c");
        LLVMBasicBlockRef else_block;
        if (statement->else_statement) else_block = append_block("if_a");
        auto end_block = append_block("if_e");
        if (!statement->else_statement) else_block = end_block;

        auto condition_value = convert_to_rvalue(statement->condition, IntegerType::of_bool(), ConvKind::IMPLICIT);
        LLVMBuildCondBr(builder, condition_value, then_block, else_block);

        LLVMPositionBuilderAtEnd(builder, then_block);
        accept_statement(statement->then_statement);
        LLVMBuildBr(builder, end_block);

        if (statement->else_statement) {
            LLVMPositionBuilderAtEnd(builder, else_block);
            accept_statement(statement->else_statement);
            LLVMBuildBr(builder, end_block);
        }

        LLVMPositionBuilderAtEnd(builder, end_block);

        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(ReturnStatement* statement) override {
        if (function_type->return_type->unqualified() == &VoidType::it) {
            if (statement->expr) {
                auto type = get_expr_type(statement->expr);
                if (type->unqualified() != &VoidType::it) {
                    message(Severity::ERROR, statement->expr->location) << "void function '" << *function_declarator->identifier << "' should not return a value\n";
                    function_declarator->message_see_declaration("return type");
                }
            }

            call_pending_destructors_at_all_scopes();
            LLVMBuildRetVoid(builder);
        } else {
            LLVMValueRef rvalue;
            if (statement->expr) {
                Value value = emit_expr(statement->expr, false).unqualified();
                if (value.type->unqualified() == function_type->return_type->unqualified()) {
                    rvalue = get_value(value, statement->expr->location);
                } else {
                    pend_destructor(value);
                    rvalue = convert_to_rvalue(value, function_type->return_type->unqualified(), ConvKind::IMPLICIT, statement->expr->location);
                }
            } else {
                message(Severity::ERROR, statement->location) << "non-void function '" << *function_declarator->identifier << "' should return a value\n";
                function_declarator->message_see_declaration("return type");
                rvalue = LLVMConstNull(function_type->return_type->llvm_type());
            }

            call_pending_destructors_at_all_scopes();
            LLVMBuildRet(builder, rvalue);
        }

        LLVMPositionBuilderAtEnd(builder, unreachable_block);

        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(SwitchStatement* statement) override {
        SwitchConstruct construct(this);

        construct.break_block = append_block("");

        LLVMBasicBlockRef default_block{};
        if (statement->num_defaults) {
            construct.default_label = default_block = append_block("");
        } else {
            default_block = construct.break_block;
        }

        auto expr_value = emit_full_expr(statement->expr).unqualified();
        auto switch_value = LLVMBuildSwitch(builder, get_value(expr_value, statement->expr->location), default_block, statement->cases.size());

        for (auto case_expr: statement->cases) {
            auto case_label = append_block("");
            innermost_switch->case_labels[case_expr] = case_label;

            auto case_value = fold_expr(case_expr);
            if(case_value.is_const_integer()) {
                LLVMAddCase(switch_value, case_value.get_const(), case_label);
            } else {
                message(Severity::ERROR, case_expr->location) << "case must have integer constant expression\n";
            }
        }

        LLVMPositionBuilderAtEnd(builder, unreachable_block);

        accept_statement(statement->body);

        LLVMBuildBr(builder, construct.break_block);
        LLVMPositionBuilderAtEnd(builder, construct.break_block);

        return VisitStatementOutput();
    }

    virtual VisitExpressionOutput visit(AddressExpr* expr) override {
        auto value = emit_expr(expr->expr);
        auto result_type = value.type->pointer_to();

        if (outcome == EmitOutcome::TYPE) return VisitExpressionOutput(result_type);

        if (value.kind != ValueKind::LVALUE) {
            message(Severity::ERROR, expr->location) << "cannot take address of rvalue of type '" << PrintType(value.type) <<"'\n";
            return VisitExpressionOutput(Value::of_recover(result_type));
        }

        return VisitExpressionOutput(result_type, get_address(value));
    }

    virtual VisitExpressionOutput visit(AssignExpr* expr) override {
        auto left_value = emit_expr(expr->left);

        auto result_type = left_value.type->unqualified();
        if (outcome == EmitOutcome::TYPE) return VisitExpressionOutput(result_type);

        auto right_value = emit_expr(expr->right, false);
        call_destructor_immediately(left_value);
        store_and_pend_destructor(left_value, right_value, expr->location, expr->right->location);

        return VisitExpressionOutput(left_value);
    }

    Value emit_logical_binary_operation(BinaryExpr* expr, const Value& left_value, const Location& left_location) {
        auto result_type = IntegerType::of_bool();
        if (outcome == EmitOutcome::TYPE) return Value(result_type);

        if (outcome == EmitOutcome::FOLD) {
            auto llvm_left = convert_to_rvalue(left_value, IntegerType::of_bool(), ConvKind::IMPLICIT, left_location);
            auto llvm_right = convert_to_rvalue(expr->right, IntegerType::of_bool(), ConvKind::IMPLICIT);

            if (expr->op == TOK_AND_OP) {
                return Value(result_type, LLVMBuildAnd(builder, llvm_left, llvm_right, ""));
            } else {
                return Value(result_type, LLVMBuildOr(builder, llvm_left, llvm_right, ""));
            }
        }

        LLVMBasicBlockRef alt_blocks[2] = {
            LLVMGetInsertBlock(builder),
            append_block(""),
        };
        auto merge_block = append_block("");

        LLVMValueRef alt_values[2];
        auto condition_rvalue = convert_to_rvalue(left_value, IntegerType::of_bool(), ConvKind::IMPLICIT, left_location);
        alt_values[0] = condition_rvalue;

        LLVMBasicBlockRef then_block = expr->op == TOK_AND_OP ? alt_blocks[1] : merge_block;
        LLVMBasicBlockRef else_block = expr->op == TOK_AND_OP ? merge_block : alt_blocks[1];
        LLVMBuildCondBr(builder, alt_values[0], then_block, else_block);

        LLVMPositionBuilderAtEnd(builder, alt_blocks[1]);
        alt_values[1] = convert_to_rvalue(expr->right, result_type, ConvKind::IMPLICIT);
        LLVMBuildBr(builder, merge_block);

        LLVMPositionBuilderAtEnd(builder, merge_block);
        Value result;
        auto phi_value = LLVMBuildPhi(builder, result_type->llvm_type(), "");
        LLVMAddIncoming(phi_value, alt_values, alt_blocks, 2);
        return Value(result_type, phi_value);
    }

    const Type* promote_integer(const Type* type) {
        auto int_type = IntegerType::default_type();

        while (auto enum_type = unqualified_type_cast<EnumType>(type)) {
            type = enum_type->base_type;
        }

        if (auto type_as_int = unqualified_type_cast<IntegerType>(type)) {
            // Integer types smaller than int are promoted when an operation is performed on them.
            if (type_as_int->size < int_type->size || (type_as_int->signedness == IntegerSignedness::SIGNED && type_as_int->size == int_type->size)) {
                // If all values of the original type can be represented as an int, the value of the smaller type is converted to an int; otherwise, it is converted to an unsigned int.
                return int_type;
            }
        }
    
        return type;
    }

    // This is applied to binary expressions and to the second and third operands of a conditional expression.
    const Type* usual_arithmetic_conversions(const Type* left, const Type* right) {
        left = promote_integer(left);
        right = promote_integer(right);

        // If both operands have the same type, no further conversion is needed.
        if (left == right) return left;

        auto left_as_float = unqualified_type_cast<FloatingPointType>(left);
        auto right_as_float = unqualified_type_cast<FloatingPointType>(right);
        if (left_as_float) {
            if (right_as_float) {
                return FloatingPointType::of(max(left_as_float->size, right_as_float->size));
            }
            return left_as_float;
        }
        if (right_as_float) return right_as_float;

        auto left_as_int = unqualified_type_cast<IntegerType>(left);
        auto right_as_int = unqualified_type_cast<IntegerType>(right);

        if (left_as_int && right_as_int) {
            // char was promoted to signed int so DEFAULT is impossible.
            assert(left_as_int ->signedness != IntegerSignedness::DEFAULT);
            assert(right_as_int->signedness != IntegerSignedness::DEFAULT);

            // If both operands are of the same integer type (signed or unsigned), the operand with the type of lesser integer conversion rank is converted to the type of the operand with greater rank.
            if (left_as_int->signedness == right_as_int->signedness) {
                return IntegerType::of(left_as_int->signedness, max(left_as_int->size, right_as_int->size));
            }

            // If the operand that has unsigned integer type has rank greater than or equal to the rank of the type of the other operand, the operand with signed integer type is converted to the type of the operand with unsigned integer type.
            auto unsigned_int = left_as_int->signedness == IntegerSignedness::UNSIGNED ? left_as_int : right_as_int;
            auto signed_int = left_as_int->signedness == IntegerSignedness::SIGNED ? left_as_int : right_as_int;
            if (unsigned_int->size >= signed_int->size) {
                return unsigned_int;
            }

            // If the type of the operand with signed integer type can represent all of the values of the type of the operand with unsigned integer type, the operand with unsigned integer type is converted to the type of the operand with signed integer type.
            if (signed_int->size > unsigned_int->size) {
                return signed_int;
            }

            // Otherwise, both operands are converted to the unsigned integer type corresponding to the type of the operand with signed integer type.
            return IntegerType::of(IntegerSignedness::UNSIGNED, signed_int->size);
        }

        return nullptr;
    }
    
    LLVMIntPredicate llvm_int_predicate(bool is_signed, TokenKind op) {
        switch (op) {
            case '<':       return is_signed ? LLVMIntSLT : LLVMIntULT;
            case '>':       return is_signed ? LLVMIntSGT : LLVMIntUGT;
            case TOK_LE_OP: return is_signed ? LLVMIntSLE : LLVMIntULE;
            case TOK_GE_OP: return is_signed ? LLVMIntSGE : LLVMIntUGE;
            case TOK_EQ_OP: return LLVMIntEQ;
            case TOK_NE_OP: return LLVMIntNE;
        }
        return LLVMIntPredicate(0);
    }

    LLVMRealPredicate llvm_float_predicate(TokenKind op) {
        switch (op) {
            case '<':       return LLVMRealOLT;
            case '>':       return LLVMRealOGT;
            case TOK_LE_OP: return LLVMRealOLE;
            case TOK_GE_OP: return LLVMRealOGE;
            case TOK_EQ_OP: return LLVMRealOEQ;
            case TOK_NE_OP: return LLVMRealONE;
        }
        return LLVMRealPredicate(0);
    }

    Value emit_scalar_binary_operation(BinaryExpr* expr, Value left_value, Value right_value, const Location& left_location, const Location& right_location) {
        OperatorFlags op_flags = operator_flags(expr->op);
        auto convert_type = (op_flags & OP_AS_LEFT_RESULT) ? left_value.type : usual_arithmetic_conversions(left_value.type, right_value.type);
        if (!convert_type) return Value();

        auto result_type = convert_type;
        if (op_flags & OP_BOOL_RESULT) {
            result_type = IntegerType::of_bool();
        }

        if (outcome == EmitOutcome::TYPE) return Value(result_type);

        auto left_rvalue = convert_to_rvalue(left_value, convert_type, ConvKind::IMPLICIT, left_location);
        auto right_rvalue = convert_to_rvalue(right_value, convert_type, ConvKind::IMPLICIT, right_location);

        if (auto as_int = unqualified_type_cast<IntegerType>(convert_type)) {
            switch (expr->op) {
              default:
                assert(false); // TODO
                break;
              case '+':
              case TOK_ADD_ASSIGN:
                return Value(result_type, LLVMBuildAdd(builder, left_rvalue, right_rvalue, ""));
              case '-':
              case TOK_SUB_ASSIGN:
                return Value(result_type, LLVMBuildSub(builder, left_rvalue, right_rvalue, ""));
              case '*':
              case TOK_MUL_ASSIGN:
                return Value(result_type, LLVMBuildMul(builder, left_rvalue, right_rvalue, ""));
              case '/':
              case TOK_DIV_ASSIGN:
                if (as_int->is_signed()) {
                    return Value(result_type, LLVMBuildSDiv(builder, left_rvalue, right_rvalue, ""));
                } else {
                    return Value(result_type, LLVMBuildUDiv(builder, left_rvalue, right_rvalue, ""));
                }
              case '&':
              case TOK_AND_ASSIGN:
                return Value(result_type, LLVMBuildAnd(builder, left_rvalue, right_rvalue, ""));
              case '|':
              case TOK_OR_ASSIGN:
                return Value(result_type, LLVMBuildOr(builder, left_rvalue, right_rvalue, ""));
              case '^':
              case TOK_XOR_ASSIGN:
                return Value(result_type, LLVMBuildXor(builder, left_rvalue, right_rvalue, ""));
              case TOK_LEFT_OP:
              case TOK_LEFT_ASSIGN:
                return Value(result_type, LLVMBuildShl(builder, left_rvalue, right_rvalue, ""));
              case TOK_RIGHT_OP:
              case TOK_RIGHT_ASSIGN:
                if (as_int->is_signed()) {
                  return Value(result_type, LLVMBuildAShr(builder, left_rvalue, right_rvalue, ""));
                } else {
                  return Value(result_type, LLVMBuildLShr(builder, left_rvalue, right_rvalue, ""));
                }
              case '<':       
              case '>':       
              case TOK_LE_OP: 
              case TOK_GE_OP: 
              case TOK_EQ_OP: 
              case TOK_NE_OP:
                return Value(result_type, LLVMBuildICmp(builder,
                                                        llvm_int_predicate(as_int->is_signed(), expr->op),
                                                        left_rvalue, right_rvalue,
                                                        ""));
            }
        }

        if (auto as_float = unqualified_type_cast<FloatingPointType>(convert_type)) {
            switch (expr->op) {
              default:
                assert(false); // TODO
              case '+':
              case TOK_ADD_ASSIGN:
                return Value(result_type, LLVMBuildFAdd(builder, left_rvalue, right_rvalue, ""));
              case '-':
              case TOK_SUB_ASSIGN:
                return Value(result_type, LLVMBuildFSub(builder, left_rvalue, right_rvalue, ""));
              case '*':
              case TOK_MUL_ASSIGN:
                return Value(result_type, LLVMBuildFMul(builder, left_rvalue, right_rvalue, ""));
              case '/':
              case TOK_DIV_ASSIGN:
                return Value(result_type, LLVMBuildFDiv(builder, left_rvalue, right_rvalue, ""));
              case '<':       
              case '>':       
              case TOK_LE_OP: 
              case TOK_GE_OP: 
              case TOK_EQ_OP: 
              case TOK_NE_OP:
                return Value(result_type, LLVMBuildFCmp(builder,
                                                        llvm_float_predicate(expr->op),
                                                        left_rvalue, right_rvalue,
                                                        ""));
            }
        }

        return Value();
    }

    Value emit_pointer_binary_operation(BinaryExpr* expr, Value left_value, Value right_value, Location left_location, Location right_location) {
        auto op = expr->op;
        auto op_flags = operator_flags(op);

        auto llvm_target_data = TranslationUnitContext::it->llvm_target_data;
        auto left_pointer_type = unqualified_type_cast<PointerType>(left_value.type);
        auto right_pointer_type = unqualified_type_cast<PointerType>(right_value.type);

        if (left_pointer_type && right_pointer_type) {
            if (op == '-' && left_pointer_type->base_type->unqualified() == right_pointer_type->base_type->unqualified()) {
                auto result_type = IntegerType::of_size(IntegerSignedness::SIGNED);
                if (outcome == EmitOutcome::TYPE) return Value(result_type);
            
                auto left_int = LLVMBuildPtrToInt(builder, get_value(left_value, left_location), result_type->llvm_type(), "");
                auto right_int = LLVMBuildPtrToInt(builder, get_value(right_value, right_location), result_type->llvm_type(), "");
                auto byte_diff = LLVMBuildSub(builder, left_int, right_int, "");
                auto size_of_base_type = LLVMStoreSizeOfType(llvm_target_data, left_pointer_type->base_type->llvm_type());
                auto result = LLVMBuildSDiv(builder, byte_diff, LLVMConstInt(result_type->llvm_type(), size_of_base_type, true), "");
                return Value(result_type, result);
            }
        }

        if ((op_flags & OP_COMMUTATIVE) && !left_pointer_type) {
            swap(left_value, right_value);
            swap(left_location, right_location);
            swap(left_pointer_type, right_pointer_type);
        }

        if (op_flags & OP_COMPARISON) {
            bool valid = false;
            if (left_pointer_type && right_pointer_type) {
                valid = check_pointer_conversion(left_pointer_type->base_type, right_pointer_type->base_type) == ConvKind::IMPLICIT
                     || check_pointer_conversion(right_pointer_type->base_type, left_pointer_type->base_type) == ConvKind::IMPLICIT;
            }

            if (op == TOK_EQ_OP || op == TOK_NE_OP) {
                if (right_value.is_null_literal) {
                    right_value = Value(left_pointer_type, LLVMConstNull(left_pointer_type->llvm_type()));
                    valid = true;
                }
            }

            if (!valid) return Value();

            auto result_type = IntegerType::of_bool();
                
            if (outcome == EmitOutcome::TYPE) return Value(result_type);

            return Value(result_type, LLVMBuildICmp(builder,
                                                    llvm_int_predicate(false, op),
                                                    get_value(left_value, left_location), get_value(right_value, right_location),
                                                    ""));
        }

        if (op == '+'|| op == TOK_ADD_ASSIGN || op == '-' || op == TOK_SUB_ASSIGN) {
            right_value = convert_to_type(right_value, promote_integer(right_value.type), ConvKind::IMPLICIT, right_location);
            if (!unqualified_type_cast<IntegerType>(right_value.type)) return Value();

            if (outcome == EmitOutcome::TYPE) return Value(left_pointer_type);

            LLVMValueRef index = get_value(right_value, right_location);

            if (op == '-' || op == TOK_SUB_ASSIGN) {
                index = LLVMBuildNeg(builder, index, "");
            }

            return Value(left_pointer_type, LLVMBuildGEP2(builder, left_pointer_type->base_type->llvm_type(), get_value(left_value, left_location), &index, 1, ""));
        }

        return Value();
    }

    Value convert_array_to_pointer(const Value& value) {
        auto zero = TranslationUnitContext::it->zero_size;

        if (auto array_type = unqualified_type_cast<ArrayType>(value.type)) {
            auto result_type = array_type->element_type->pointer_to();
            if (outcome == EmitOutcome::TYPE) return Value(result_type);

            LLVMValueRef indices[2] = {zero, zero};
            return Value(result_type,
                         LLVMBuildGEP2(builder, array_type->llvm_type(), get_address(value), indices, 2, ""));
        }

        return value;
    }

    virtual VisitExpressionOutput visit(BinaryExpr* expr) override {
        auto op = expr->op;
        Value intermediate;
        Value left_value = emit_expr(expr->left).unqualified();

        if (op == TOK_AND_OP || op == TOK_OR_OP) {
            intermediate = emit_logical_binary_operation(expr, left_value, expr->left->location);
        } else {
            left_value = convert_array_to_pointer(left_value);
            auto left_pointer_type = unqualified_type_cast<PointerType>(left_value.type);

            auto right_value = emit_expr(expr->right).unqualified();
            right_value = convert_array_to_pointer(right_value);
            auto right_pointer_type = unqualified_type_cast<PointerType>(right_value.type);

            if (left_pointer_type || right_pointer_type) {
                intermediate = emit_pointer_binary_operation(expr, left_value, right_value, expr->left->location, expr->right->location);
            } else {
                intermediate = emit_scalar_binary_operation(expr, left_value, right_value, expr->left->location, expr->right->location);
            }
        }

        if (!intermediate.is_valid()) {
            auto left_type = get_expr_type(expr->left);
            auto right_type = get_expr_type(expr->right);
            auto& stream = message(Severity::ERROR, expr->location) << "'" << expr->message_kind() << "' operation may not be evaluated with operands of types '"
                                                                           << PrintType(left_type) << "' and '" << PrintType(right_type) << "'\n";
            pause_messages();
            intermediate = Value::of_zero_int();
        }

        if (outcome == EmitOutcome::TYPE) return VisitExpressionOutput(intermediate);

        if (operator_flags(expr->op) & OP_ASSIGN) {
            store(left_value, get_value(intermediate, expr->location), expr->location);
            return VisitExpressionOutput(left_value);
        }

        return VisitExpressionOutput(intermediate);
    }

    virtual VisitExpressionOutput visit(CallExpr* expr) override {
        if (outcome == EmitOutcome::FOLD) {
            message(Severity::ERROR, expr->location) << "cannot call function in constant expression\n";
            throw FoldError(true);
        }

        auto function_value = emit_expr(expr->function).unqualified();

        if (auto pointer_type = unqualified_type_cast<PointerType>(function_value.type)) {
            function_value = Value(ValueKind::LVALUE, pointer_type->base_type, get_value(function_value, expr->function->location));
        }

        if (auto function_type = unqualified_type_cast<FunctionType>(function_value.type)) {
            auto result_type = function_type->return_type;
            if (outcome == EmitOutcome::TYPE) return VisitExpressionOutput(result_type);

            Declarator* function_declarator{};
            if (auto entity_expr = dynamic_cast<EntityExpr*>(expr->function)) {
                function_declarator = entity_expr->declarator;
            }

            size_t actual_num_params = expr->parameters.size();

            Value object_value;
            MemberExpr* member_expr = dynamic_cast<MemberExpr*>(expr->function);
            if (member_expr) {
                function_declarator = member_expr->member;
                object_value = emit_object_of_member_expr(member_expr);
                ++actual_num_params;
            }

            size_t expected_num_params = function_type->parameter_types.size();
            if (actual_num_params != expected_num_params) {
                message(Severity::ERROR, expr->location) << "expected " << expected_num_params << " parameter(s) but got " << actual_num_params << "\n";

                if (function_declarator) {
                    function_declarator->message_see_declaration("prototype");
                }

                return VisitExpressionOutput(Value::of_recover(result_type));
            }

            vector<LLVMValueRef> llvm_params;
            llvm_params.resize(expected_num_params);

            size_t param_expr_idx{};
            for (size_t i = 0; i < expected_num_params; ++i) {
                Value param_value;
                auto expected_type = function_type->parameter_types[i];

                const PassByReferenceType* pass_by_ref_type{};
                if (pass_by_ref_type = unqualified_type_cast<PassByReferenceType>(expected_type)) {
                    expected_type = pass_by_ref_type->base_type;
                }

                Location param_location;
                if (member_expr && i == 0) {
                    param_value = object_value;
                    param_location = member_expr->object->location;
                } else {
                    auto param_expr = expr->parameters[param_expr_idx++];
                    param_value = emit_expr(param_expr);
                    param_location = param_expr->location;
                }

                if (pass_by_ref_type) {
                    if (param_value.kind == ValueKind::RVALUE && (pass_by_ref_type->kind == PassByReferenceType::Kind::RVALUE || expected_type->qualifiers() & QUALIFIER_CONST || (member_expr && i == 0))) {
                        param_value = convert_to_type(param_value.unqualified(), expected_type, ConvKind::IMPLICIT, param_location);
                        make_addressable(param_value);
                        llvm_params[i] = get_address(param_value);
                    } else if (param_value.kind != ValueKind::LVALUE) {
                        message(Severity::ERROR, param_location) << "rvalue type '" << PrintType(param_value.type) << "' incompatible with non-const pass-by-reference parameter type '"
                                                                 << PrintType(function_type->parameter_types[i]) << "'\n";
                        llvm_params[i] = LLVMConstNull(pass_by_ref_type->llvm_type());
                    } else {
                        if (pass_by_ref_type->kind == PassByReferenceType::Kind::RVALUE && param_value.kind == ValueKind::LVALUE) {
                            message(Severity::ERROR, param_location) << "cannot pass lvalue to pass-by-rvalue-reference parameter type '"
                                                                     << PrintType(function_type->parameter_types[i]) << "'; consider postfix '&&' move expression\n";
                        } else if (param_value.type->unqualified() != expected_type->unqualified()) {
                            message(Severity::ERROR, param_location) << "lvalue type '" << PrintType(param_value.type) << "' incompatible with pass-by-reference parameter type '"
                                                                     << PrintType(function_type->parameter_types[i]) << "'\n";
                        } else if (param_value.qualifiers > expected_type->qualifiers()) {
                            message(Severity::ERROR, param_location) << "lvalue type '" << PrintType(param_value.type) << "' has more type qualifiers than pass-by-reference parameter type '"
                                                                     << PrintType(function_type->parameter_types[i]) << "'\n";
                        }

                        param_value = param_value.unqualified();
                        llvm_params[i] = get_address(param_value);
                    }
                } else {
                    param_value = convert_to_type(param_value.unqualified(), expected_type, ConvKind::IMPLICIT, param_location);
                    llvm_params[i] = get_value(param_value, param_location);
                }
            }

            return VisitExpressionOutput(result_type, LLVMBuildCall2(builder, function_type->llvm_type(), get_address(function_value), llvm_params.data(), llvm_params.size(), ""));
        }

        message(Severity::ERROR, expr->location) << "type '" << PrintType(function_value.type) << "' is not a function or function pointer\n";

        return VisitExpressionOutput();
    }

    virtual VisitExpressionOutput visit(CastExpr* expr) override {
        if (outcome == EmitOutcome::TYPE) return VisitExpressionOutput(expr->type);

        auto value = convert_to_type(expr->expr, expr->type, ConvKind::EXPLICIT);
        return VisitExpressionOutput(value);
    }

    virtual VisitExpressionOutput visit(ConditionExpr* expr) override {
        auto context = TranslationUnitContext::it;

        auto then_type = get_expr_type(expr->then_expr)->unqualified();
        auto else_type = get_expr_type(expr->else_expr)->unqualified();

        auto result_type = usual_arithmetic_conversions(then_type, else_type);
        if (!result_type) {
            // TODO there are other combinations of types that are valid for condition expressions
            message(Severity::ERROR, expr->location) << "incompatible conditional operand types '" << PrintType(then_type) << "' and '" << PrintType(else_type) << "'\n";
            pause_messages();
            return VisitExpressionOutput(Value::of_zero_int());
        }

        if (outcome == EmitOutcome::TYPE) return VisitExpressionOutput(result_type);

        auto condition_rvalue = convert_to_rvalue(expr->condition, IntegerType::of(IntegerSignedness::UNSIGNED, IntegerSize::BOOL), ConvKind::IMPLICIT);

        if (outcome == EmitOutcome::FOLD) {
            return VisitExpressionOutput(result_type, LLVMBuildSelect(builder, condition_rvalue,
                                                                      convert_to_rvalue(expr->then_expr, result_type, ConvKind::IMPLICIT),
                                                                      convert_to_rvalue(expr->else_expr, result_type, ConvKind::IMPLICIT),
                                                                      ""));
        }

        if (condition_rvalue == context->llvm_true) {
            return VisitExpressionOutput(convert_to_type(expr->then_expr, result_type, ConvKind::IMPLICIT));
        } else if (condition_rvalue == context->llvm_false) {
            return VisitExpressionOutput(convert_to_type(expr->else_expr, result_type, ConvKind::IMPLICIT));
        }

        LLVMBasicBlockRef alt_blocks[2] = {
            append_block("then"),
            append_block("else"),
        };
        auto merge_block = append_block("merge");

        LLVMBuildCondBr(builder, condition_rvalue, alt_blocks[0], alt_blocks[1]);

        LLVMValueRef alt_values[2];

        LLVMPositionBuilderAtEnd(builder, alt_blocks[0]);
        auto then_rvalue = convert_to_rvalue(expr->then_expr, result_type, ConvKind::IMPLICIT);
        if (result_type != &VoidType::it) alt_values[0] = then_rvalue;
        LLVMBuildBr(builder, merge_block);

        LLVMPositionBuilderAtEnd(builder, alt_blocks[1]);
        auto else_rvalue = convert_to_rvalue(expr->else_expr, result_type, ConvKind::IMPLICIT);
        if (result_type != &VoidType::it) alt_values[1] = else_rvalue;
        LLVMBuildBr(builder, merge_block);

        LLVMPositionBuilderAtEnd(builder, merge_block);
        Value result;
        if (result_type != &VoidType::it) {
            auto phi_value = LLVMBuildPhi(builder, result_type->llvm_type(), "");
            LLVMAddIncoming(phi_value, alt_values, alt_blocks, 2);
            result = Value(result_type, phi_value);
        } else {
            result = Value(result_type);
        }

        return VisitExpressionOutput(result);
    }

    virtual VisitExpressionOutput visit(DereferenceExpr* expr) override {

        auto value = emit_expr(expr->expr).unqualified();
        auto pointer_type = unqualified_type_cast<PointerType>(value.type);
        auto result_type = pointer_type->base_type;
        if (outcome == EmitOutcome::TYPE) return VisitExpressionOutput(result_type);

        return VisitExpressionOutput(Value(ValueKind::LVALUE, result_type, get_value(value, expr->location)));
    }

    virtual VisitExpressionOutput visit(EntityExpr* expr) override {
        auto declarator = expr->declarator->primary;
        auto result_type = declarator->type;

        if (outcome == EmitOutcome::TYPE) return VisitExpressionOutput(result_type);

        if (auto enum_constant = declarator->enum_constant()) {
            if (!enum_constant->ready) {
                message(Severity::ERROR, expr->location) << "enum constant '" << *declarator->identifier << "' not yet available\n";
                declarator->message_see_declaration();
            }

            auto type = result_type;
            while (auto enum_type = unqualified_type_cast<EnumType>(type->unqualified())) {
                type = enum_type->base_type;
            }

            auto int_type = unqualified_type_cast<IntegerType>(type);

            return VisitExpressionOutput(Value::of_int(int_type, enum_constant->value).bit_cast(result_type));

        }

        if (auto variable = declarator->variable()) {
            if (outcome == EmitOutcome::FOLD && variable->initializer && (declarator->type->qualifiers() & QUALIFIER_CONST)) {
                try {
                    ScopedMessagePauser pauser;
                    return VisitExpressionOutput(convert_to_type(emit_expr(variable->initializer), result_type, ConvKind::IMPLICIT, variable->initializer->location));
                } catch (FoldError&) {
                }
            }

            if (outcome == EmitOutcome::IR && this_value.is_valid() && !variable->value.is_valid()) {
                if (auto this_type = unqualified_type_cast<StructuredType>(this_value.type->unqualified())) {
                    assert(variable->member);

                    if (declarator->scope != this_type->scope) {
                        message(Severity::ERROR, expr->location) << "'" << *declarator->identifier << "' is not a member of the immediately enclosing type so is inaccessible via 'this'\n";
                        declarator->message_see_declaration();
                        pause_messages();
                    } else {
                        return VisitExpressionOutput(Value(ValueKind::LVALUE, declarator->type,
                            LLVMBuildGEP2(builder, this_type->llvm_type(), get_address(this_value), variable->member->gep_indices.data(), variable->member->gep_indices.size(), "")));
                    }
                }
            }
        }
        
        if (auto entity = declarator->entity()) {
            if (entity->value.kind == ValueKind::LVALUE) {
                return VisitExpressionOutput(entity->value);
            }
        }

        if (outcome == EmitOutcome::FOLD) {
            message(Severity::ERROR, expr->location) << declarator->message_kind() << " '" << *declarator->identifier << "' is not a constant\n";
            declarator->message_see_declaration();
            throw FoldError(true);
        } else {
            message(Severity::ERROR, expr->location) << declarator->message_kind() << " '" << *declarator->identifier << "' is not an expression\n";
            declarator->message_see_declaration();
            pause_messages();
        }

        return VisitExpressionOutput(Value::of_recover(declarator->type));
    }

    virtual VisitExpressionOutput visit(IncDecExpr* expr) override {
        auto lvalue = emit_expr(expr->expr).unqualified();
        auto before_rvalue = get_value(lvalue, expr->location);

        const Type* result_type = lvalue.type;
        if (outcome == EmitOutcome::TYPE) return VisitExpressionOutput(result_type);

        LLVMValueRef one = LLVMConstInt(result_type->llvm_type(), 1, false);

        LLVMValueRef after_rvalue;
        switch (expr->op) {
          case TOK_INC_OP:
            after_rvalue = LLVMBuildAdd(builder, before_rvalue, one, "");
            break;
          case TOK_DEC_OP:
            after_rvalue = LLVMBuildSub(builder, before_rvalue, one, "");
            break;
        }

        store(lvalue, after_rvalue, expr->location);

        return VisitExpressionOutput(expr->postfix ? Value(result_type, before_rvalue) : lvalue);
    }

    Value emit_object_of_member_expr(MemberExpr* expr) {
        auto object = emit_expr(expr->object);
        if (auto pointer_type = unqualified_type_cast<PointerType>(object.type)) {
            object = Value(ValueKind::LVALUE, pointer_type->base_type, get_value(object, expr->object->location));
        }
        return object;
    }

    virtual VisitExpressionOutput visit(MemberExpr* expr) override {
        if (!expr->member) return VisitExpressionOutput(Value::of_zero_int());

        auto object_type = get_expr_type(expr->object)->unqualified();

        if (auto pointer_type = unqualified_type_cast<PointerType>(object_type)) {
            object_type = pointer_type->base_type->unqualified();
        }

        if (auto struct_type = unqualified_type_cast<StructuredType>(object_type)) {
            VisitExpressionOutput output;

            auto result_type = expr->member->type;
            if (outcome == EmitOutcome::TYPE) return VisitExpressionOutput(result_type);

            if (auto member_variable = expr->member->variable()) {
                auto object = emit_object_of_member_expr(expr);
                
                auto llvm_struct_type = struct_type->llvm_type();

                output.value = Value(ValueKind::LVALUE, result_type, LLVMBuildInBoundsGEP2(builder, llvm_struct_type, get_address(object),
                                                                                  member_variable->member->gep_indices.data(), member_variable->member->gep_indices.size(),
                                                                                  expr->identifier.c_str()));
                output.value.bit_field = member_variable->member->bit_field.get();
                return output;
            }
            
            if (auto member_entity = expr->member->entity()) {
                assert(member_entity->value.is_valid());
                output.value = member_entity->value;

                return output;
            }
        }

        return VisitExpressionOutput(Value::of_zero_int());
    }

    virtual VisitExpressionOutput visit(MoveExpr* expr) override {
        auto value = emit_expr(expr->expr).unqualified();
        if (outcome == EmitOutcome::TYPE) return VisitExpressionOutput(value.type);

        auto result = Value(value.type, get_value(value, expr->expr->location, true));
        return VisitExpressionOutput(result);
    }

    virtual VisitExpressionOutput visit(SequenceExpr* expr) override {
        auto left_value = emit_expr(expr->left).unqualified();
        auto right_value = emit_expr(expr->right, false).unqualified();
        return VisitExpressionOutput(right_value);
    }

    virtual VisitExpressionOutput visit(SizeOfExpr* expr) override {
        auto zero = TranslationUnitContext::it->zero_size;
        auto llvm_target_data = TranslationUnitContext::it->llvm_target_data;

        auto result_type = IntegerType::of_size(IntegerSignedness::UNSIGNED);

        if (expr->type->partition() == TypePartition::INCOMPLETE) {
            return VisitExpressionOutput(result_type, zero);
        }

        if (outcome == EmitOutcome::TYPE) return VisitExpressionOutput(result_type);

        return VisitExpressionOutput(result_type, Value::of_size(LLVMStoreSizeOfType(llvm_target_data, expr->type->llvm_type())).get_const());
    }

    virtual VisitExpressionOutput visit(SubscriptExpr* expr) override {
        auto zero = TranslationUnitContext::it->zero_size;

        auto left_value = emit_expr(expr->left).unqualified();
        auto index_value = emit_expr(expr->right).unqualified();

        if (unqualified_type_cast<IntegerType>(left_value.type)) {
            swap(left_value, index_value);
        }

        if (auto pointer_type = unqualified_type_cast<PointerType>(left_value.type)) {
            auto result_type = pointer_type->base_type;
            if (outcome == EmitOutcome::TYPE) return VisitExpressionOutput(result_type);

            LLVMValueRef index = get_value(index_value, expr->right->location);

            return VisitExpressionOutput(Value(
                ValueKind::LVALUE,
                result_type,
                LLVMBuildGEP2(builder, result_type->llvm_type(), get_value(left_value, expr->left->location), &index, 1, "")));
        }

        if (auto array_type = unqualified_type_cast<ArrayType>(left_value.type)) {
            auto result_type = array_type->element_type;
            if (outcome == EmitOutcome::TYPE) return VisitExpressionOutput(result_type);

            LLVMValueRef indices[2] = { zero, get_value(index_value, expr->right->location) };

            return VisitExpressionOutput(Value(
                ValueKind::LVALUE,
                result_type,
                LLVMBuildGEP2(builder, array_type->llvm_type(), get_address(left_value), indices, 2, "")));
        }

        return VisitExpressionOutput();
    }

    virtual VisitExpressionOutput visit(IntegerConstant* constant) override {
        return VisitExpressionOutput(constant->value);
    }

    virtual VisitExpressionOutput visit(FloatingPointConstant* constant) override {
        return VisitExpressionOutput(constant->value);
    }

    virtual VisitExpressionOutput visit(StringConstant* constant) override {
        auto result_type = ResolvedArrayType::of(ArrayKind::COMPLETE,
                                                 QualifiedType::of(constant->character_type, QUALIFIER_CONST),
                                                 constant->value.length + 1);
        if (outcome == EmitOutcome::TYPE) return VisitExpressionOutput(result_type);

        auto llvm_context = TranslationUnitContext::it->llvm_context;

        LLVMValueRef llvm_constant{};
        if (constant->character_type == IntegerType::of_char(false)) {
            llvm_constant = LLVMConstStringInContext(llvm_context, constant->value.chars.data(), constant->value.chars.size(), false);
        } else {
            LLVMTypeRef llvm_char_type = constant->character_type->llvm_type();
            vector<LLVMValueRef> llvm_char_values;
            string_view source(constant->value.chars);
            llvm_char_values.reserve(constant->value.length + 1);
            while (source.size()) {
                auto c = decode_char(source);
                llvm_char_values.push_back(LLVMConstInt(llvm_char_type, c.code, false));
            }
            llvm_char_values.push_back(LLVMConstInt(llvm_char_type, 0, false));
            llvm_constant = LLVMConstArray(llvm_char_type, llvm_char_values.data(), llvm_char_values.size());
        }

        return VisitExpressionOutput(result_type, llvm_constant);
    }

    void accept_scope(Scope* scope) {
        for (auto declarator: scope->declarators) {
            accept_declarator(declarator);
        }
    }
};

EmitterScope::EmitterScope(Emitter* emitter): emitter(emitter) {
    parent_scope = emitter->innermost_scope;
    emitter->innermost_scope = this;
}

EmitterScope::~EmitterScope() {
    emitter->call_pending_destructors(*this);
    emitter->innermost_scope = parent_scope;
}

Construct::Construct(Emitter* emitter): EmitterScope(emitter) {
}

SwitchConstruct::SwitchConstruct(Emitter* emitter): Construct(emitter) {
    parent_switch = emitter->innermost_switch;
    emitter->innermost_switch = this;
}

SwitchConstruct::~SwitchConstruct() {
    emitter->innermost_switch = parent_switch;
}

const Type* get_expr_type(const Expr* expr) {
    static const EmitOptions options;
    Emitter emitter(nullptr, EmitOutcome::TYPE, options);
    return emitter.emit_expr(const_cast<Expr*>(expr)).type;
}

Value fold_expr(const Expr* expr) {
    static const EmitOptions options;
    Emitter emitter(nullptr, EmitOutcome::FOLD, options);
    try {
        return emitter.emit_expr(const_cast<Expr*>(expr));
    } catch (FoldError&) {
        return Value::of_recover(get_expr_type(expr));
    }
}

void Module::emit_pass(const EmitOptions& options) {
    Emitter emitter(this, EmitOutcome::IR, options);

    emitter.accept_scope(file_scope);
    for (auto scope: type_scopes) {
        emitter.accept_scope(scope);
    }

    LLVMVerifyModule(llvm_module, LLVMPrintMessageAction, nullptr);
}
