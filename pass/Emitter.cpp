#include "Module.h"
#include "lex/StringLiteral.h"
#include "LLVM.h"
#include "Message.h"
#include "parse/AssocPrec.h"
#include "parse/Declaration.h"
#include "TranslationUnitContext.h"
#include "TypeConverter.h"
#include "TypeVisitor.h"
#include "Visitor.h"

struct Emitter;

struct Construct {
    Construct* parent{};
    Emitter* emitter{};

    LLVMBasicBlockRef continue_block{};
    LLVMBasicBlockRef break_block{};

    Construct(Emitter* emitter);
    ~Construct();
};

struct SwitchConstruct: Construct {
    SwitchConstruct* parent_switch{};

    unordered_map<Expr*, LLVMBasicBlockRef> case_labels;
    LLVMBasicBlockRef default_label{};

    SwitchConstruct(Emitter* emitter);
    ~SwitchConstruct();
};

struct PendingDestructor {
    Value lvalue;
    Declarator* destructor_declarator{};
};

struct EmitterScope {
    vector<PendingDestructor> destructors;
};

struct Emitter: Visitor {
    EmitOutcome outcome;
    const EmitOptions& options;

    Module* module{};
    Emitter* parent{};

    Declarator* function_declarator{};
    const FunctionType* function_type{};
    list<EmitterScope> scopes;
    LLVMBuilderRef builder{};
    LLVMBuilderRef temp_builder{};
    LLVMValueRef function{};
    LLVMBasicBlockRef entry_block{};
    LLVMBasicBlockRef unreachable_block{};
    unordered_map<InternedString, LLVMBasicBlockRef> goto_labels;
    Construct* innermost_construct{};
    SwitchConstruct* innermost_switch{};
    InternedString this_string;
    Value this_value;
    LLVMTypeRef destructor_wrapper_type{};
    LLVMValueRef destructor_wrapper{};

    Emitter(EmitOutcome outcome, const EmitOptions& options): outcome(outcome), options(options) {
        auto llvm_context = TranslationUnitContext::it->llvm_context;
        if (outcome != EmitOutcome::TYPE) {
            builder = LLVMCreateBuilderInContext(llvm_context);
        }
        if (outcome == EmitOutcome::IR) {
            temp_builder = LLVMCreateBuilderInContext(llvm_context);
        }
        this_string = intern_string("this");
    }

    ~Emitter() {
        if (builder) LLVMDisposeBuilder(builder);
        if (temp_builder) LLVMDisposeBuilder(temp_builder);
    }

    void push_scope() {
        scopes.emplace_back();
    }

    void call_destructors() {
        auto context = TranslationUnitContext::it;

        auto& destructors = scopes.back().destructors;
        for (auto it = destructors.rbegin(); it != destructors.rend(); ++it) {
            auto destructor = *it;
            auto function = destructor.destructor_declarator->function();
            auto structured_type = type_cast<StructuredType>(destructor.lvalue.type->unqualified());
            auto placeholder = module->get_destructor_placeholder(structured_type);

            LLVMValueRef args[] = {
                destructor.lvalue.dangerously_get_lvalue(),
                get_lvalue(function->value),
                destructor.lvalue.dangerously_get_rvalue(builder),
                LLVMConstNull(destructor.lvalue.type->llvm_type()),
            };
            LLVMBuildCall2(builder, placeholder.type, placeholder.function, args, 4, "");
        }

        destructors.clear();
    }

    void pop_scope() {
        call_destructors();
        scopes.pop_back();
    }

    bool pend_destructor(const Value& value) {
        auto context = TranslationUnitContext::it;

        if (auto structured_type = type_cast<StructuredType>(value.type->unqualified())) {
            if (auto destructor_declarator = structured_type->destructor) {
                auto& scope = scopes.back();
                scope.destructors.push_back(PendingDestructor(value, destructor_declarator));
                return true;
            }
        }

        return false;
    }

    virtual VisitDeclaratorOutput accept_declarator(Declarator* declarator) override {
        declarator = declarator->primary;
        if (declarator->status >= DeclaratorStatus::EMITTED) return VisitDeclaratorOutput();
        assert(declarator->status == DeclaratorStatus::RESOLVED);

        declarator->accept(*this, VisitDeclaratorInput());

        declarator->status = DeclaratorStatus::EMITTED;

        return VisitDeclaratorOutput();
    }

    // todo: not dangerous
    LLVMValueRef get_lvalue(const Value &value) {
        return value.dangerously_get_lvalue();
    }

    LLVMValueRef get_rvalue(const Value &value) {
        auto context = TranslationUnitContext::it;

        auto rvalue = value.get_rvalue(builder, outcome);

        if (auto structured_type = type_cast<StructuredType>(value.type->unqualified())) {
            if (value.kind == ValueKind::LVALUE && structured_type->destructor) {
                LLVMValueRef lvalue = value.dangerously_get_lvalue();
                LLVMBuildStore(builder, LLVMConstNull(structured_type->llvm_type()), lvalue);
            }
        }

        return rvalue;
    }

    void store_value(const Value& dest, const Value& source, const Location& location) {
        if (outcome == EmitOutcome::IR) {
            if (dest.kind == ValueKind::LVALUE) {
                dest.store(builder, source);
            } else {
                message(Severity::ERROR, location) << "expression is not assignable\n";
            }
        } else {
            message(Severity::ERROR, location) << "assignment in constant expression\n";
        }
    }

    LLVMValueRef size_const_int(unsigned long long i) {
        auto llvm_context = TranslationUnitContext::it->llvm_context;
        return LLVMConstInt(LLVMInt64TypeInContext(llvm_context), i, false);
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

    VisitExpressionOutput accept_expr(Expr* expr) {
        try {
            return expr->accept(*this);
        } catch (FoldError& e) {
            if (!e.error_reported) {
                message(Severity::ERROR, expr->location) << "not a constant expression\n";
            }
            throw FoldError(true);
        }
    }

    Value emit_full_expr(Expr* expr) {
        push_scope();
        auto value = accept_expr(expr).value;
        pop_scope();
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

    template <typename T, typename U>
    const T* type_cast(const U* type) {
        assert(type->qualifiers() == 0);
        return dynamic_cast<const T*>(type);
    }

    Value convert_to_type(Expr* expr, const Type* dest_type, ConvKind kind) {
        auto value = accept_expr(expr).value.unqualified();
        return convert_to_type(value, dest_type, kind, expr->location);
    }

    Value convert_to_type(const Value& value, const Type* dest_type, ConvKind kind, const Location& location) {
        assert(value.type->qualifiers() == 0);
        
        dest_type = dest_type->unqualified();

        if (value.is_null_literal && kind == ConvKind::IMPLICIT && type_cast<PointerType>(dest_type)) {
            return Value(dest_type, LLVMConstNull(dest_type->llvm_type()));
        }

        if (value.type == dest_type) {
            Value result = value;
            if (kind == ConvKind::EXPLICIT) result.is_null_literal = false;
            return result;
        }

        auto output = ::convert_to_type(value, dest_type, module, builder, outcome);
        auto result = output.value;

        if (!result.is_valid()) {
            message(Severity::ERROR, location) << "cannot convert from type '" << PrintType(value.type)
                                               << "' to type '" << PrintType(dest_type) << "'\n";
            pause_messages();
            if (dest_type != &VoidType::it) {
                result = Value(dest_type, LLVMConstNull(dest_type->llvm_type()));
            } else {
                return Value::of_zero_int();
            }
        } else if ((output.conv_kind != ConvKind::IMPLICIT) && kind == ConvKind::IMPLICIT) {
            auto severity = output.conv_kind == ConvKind::C_IMPLICIT ? Severity::CONTEXTUAL_ERROR : Severity::ERROR;
            message(severity, location) << "conversion from type '" << PrintType(value.type)
                                        << "' to type '" << PrintType(dest_type) << "' requires explicit cast\n";
        }

        assert(result.type == dest_type);
        return result;
    }

    void emit_function_definition(Declarator* declarator, Function* entity) {
        function_declarator = declarator;
        function_type = type_cast<FunctionType>(declarator->primary->type);
        function = get_lvalue(entity->value);

        entry_block = append_block("");
        unreachable_block = append_block("");
        LLVMPositionBuilderAtEnd(builder, entry_block);

        this_value = Value();
        for (size_t i = 0; i < entity->parameters.size(); ++i) {
            auto param = entity->parameters[i];
            auto param_entity = param->entity();

            auto llvm_param = LLVMGetParam(function, i);

            if (auto reference_type = dynamic_cast<const PassByReferenceType*>(param->type)) {
                param_entity->value = Value(ValueKind::LVALUE, reference_type->base_type, llvm_param);
            } else {
                auto storage = LLVMBuildAlloca(builder, param->type->llvm_type(), c_str(param->identifier));
                param_entity->value = Value(ValueKind::LVALUE, param->type, storage);
                LLVMBuildStore(builder, llvm_param, storage);
            }

            if (param->identifier == this_string) {
                this_value = param_entity->value;
            }
        }

        accept_statement(entity->body);

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
            if (auto array_type = type_cast<ResolvedArrayType>(dest.type)) {
                for (size_t i = 0; i < array_type->size; ++i) {
                    LLVMValueRef indices[] = { context->zero_size, size_const_int(i) };
                    LLVMValueRef dest_element = LLVMBuildGEP2(builder, array_type->llvm_type(), get_lvalue(dest), indices, 2, "");
                    emit_auto_initializer(Value(ValueKind::LVALUE, array_type->element_type, dest_element), initializer->elements[i]);
                }
                return;
            }

            if (auto struct_type = type_cast<StructType>(dest.type)) {
                size_t initializer_idx{};
                for (auto declaration: struct_type->declarations) {
                    for (auto member: declaration->declarators) {
                        if (auto member_variable = member->variable()) {
                            LLVMValueRef dest_element = LLVMBuildInBoundsGEP2(builder, struct_type->llvm_type(), get_lvalue(dest),
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
            scalar_value = convert_to_type(expr, dest.type, ConvKind::IMPLICIT);
        }

        LLVMBuildStore(builder, get_rvalue(scalar_value), get_lvalue(dest));
    }

    LLVMValueRef emit_static_initializer(const Type* dest_type, Expr* expr) {
        if (auto uninitializer = dynamic_cast<UninitializedExpr*>(expr)) {
            return LLVMGetUndef(dest_type->llvm_type());
        }

        if (auto initializer = dynamic_cast<InitializerExpr*>(expr)) {
            if (auto array_type = type_cast<ResolvedArrayType>(dest_type)) {
                vector<LLVMValueRef> values(array_type->size);
                for (size_t i = 0; i < array_type->size; ++i) {
                    values[i] = emit_static_initializer(array_type->element_type, initializer->elements[i]);
                }

                return LLVMConstArray(array_type->element_type->llvm_type(), values.data(), values.size());
            }

            if (auto struct_type = type_cast<StructType>(dest_type)) {
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

            return get_rvalue(emit_scalar_initializer(dest_type, initializer));
        }

        return get_rvalue(convert_to_type(expr, dest_type, ConvKind::IMPLICIT));
    }
    
    void use_temp_builder() {
        auto first_insn = LLVMGetFirstInstruction(entry_block);
        if (first_insn) {
            LLVMPositionBuilderBefore(temp_builder, first_insn);
        } else {
            LLVMPositionBuilderAtEnd(temp_builder, entry_block);
        }
    }

    Value allocate_auto_storage(const Type* type, const char* name) {
        use_temp_builder();
        auto storage = LLVMBuildAlloca(temp_builder, type->llvm_type(), name);
        return Value(ValueKind::LVALUE, type, storage);
    }

    void emit_variable(Declarator* declarator, Variable* entity) {
        auto type = declarator->primary->type;
        auto llvm_type = type->llvm_type();

        if (entity->storage_duration == StorageDuration::AUTO) {
            entity->value = allocate_auto_storage(type, c_str(declarator->identifier));

            bool has_destructor = pend_destructor(entity->value);

            if (entity->initializer) {
                push_scope();
                emit_auto_initializer(entity->value, entity->initializer);
                pop_scope();
            } else if (options.initialize_variables || has_destructor) {
                entity->value.store(builder, Value::of_null(type));
            }

        } else if (entity->storage_duration == StorageDuration::STATIC) {
            auto global = get_lvalue(entity->value);

            LLVMValueRef initial = LLVMConstNull(llvm_type);
            if (entity->initializer) {
                auto old_outcome = outcome;
                outcome = EmitOutcome::FOLD;
                SCOPE_EXIT {
                    outcome = old_outcome;
                };

                Value value;
                try {
                    initial = emit_static_initializer(type, entity->initializer);
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
            Emitter function_emitter(EmitOutcome::IR, options);
            function_emitter.module = module;
            function_emitter.parent = this;
            function_emitter.emit_function_definition(declarator, function);
        }

        return VisitDeclaratorOutput();
    }

    VisitStatementOutput visit(CompoundStatement* statement) {
        push_scope();

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

        pop_scope();

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

        if (statement->condition) {
            auto condition_value = convert_to_type(statement->condition, IntegerType::of_bool(), ConvKind::IMPLICIT);
            LLVMBuildCondBr(builder, get_rvalue(condition_value), body_block, end_block);
        }

        LLVMPositionBuilderAtEnd(builder, body_block);
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
        switch (statement->kind) {
          case TOK_GOTO:
            target_block = lookup_label(statement->identifier);
            break;
          case TOK_BREAK:
            target_block = innermost_construct->break_block;
            break;
          case TOK_CONTINUE:
            target_block = innermost_construct->continue_block;
            break;
        
        }
        LLVMBuildBr(builder, target_block);
        LLVMPositionBuilderAtEnd(builder, unreachable_block);

        return VisitStatementOutput();
    }

    VisitStatementOutput visit(IfElseStatement* statement) {
        auto then_block = append_block("if_c");
        LLVMBasicBlockRef else_block;
        if (statement->else_statement) else_block = append_block("if_a");
        auto end_block = append_block("if_e");
        if (!statement->else_statement) else_block = end_block;

        auto condition_value = convert_to_type(statement->condition, IntegerType::of_bool(), ConvKind::IMPLICIT);
        LLVMBuildCondBr(builder, get_rvalue(condition_value), then_block, else_block);

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

            call_destructors();
            LLVMBuildRetVoid(builder);
        } else {
            Value value;
            if (statement->expr) {
                value = convert_to_type(statement->expr, function_type->return_type->unqualified(), ConvKind::IMPLICIT);
            } else {
                message(Severity::ERROR, statement->location) << "non-void function '" << *function_declarator->identifier << "' should return a value\n";
                function_declarator->message_see_declaration("return type");
                value = Value(function_type->return_type, LLVMConstNull(function_type->return_type->llvm_type()));
            }

            auto llvm_value = get_rvalue(value);
            call_destructors();
            LLVMBuildRet(builder, llvm_value);
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
        auto switch_value = LLVMBuildSwitch(builder, get_rvalue(expr_value), default_block, statement->cases.size());

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
        auto value = accept_expr(expr->expr).value;
        auto result_type = value.type->pointer_to();
        if (outcome == EmitOutcome::TYPE) return VisitExpressionOutput(result_type);

        // todo: error if not an lvalue
        return VisitExpressionOutput(result_type, get_lvalue(value));
    }

    virtual VisitExpressionOutput visit(AssignExpr* expr) override {
        auto left_value = accept_expr(expr->left).value;
        auto result_type = left_value.type->unqualified();
        if (outcome == EmitOutcome::TYPE) return VisitExpressionOutput(result_type);

        auto right_value = convert_to_type(expr->right, result_type, ConvKind::IMPLICIT);
        store_value(left_value, right_value, expr->location);
        return VisitExpressionOutput(right_value);
    }

    Value emit_logical_binary_operation(BinaryExpr* expr, const Value& left_value, const Location& left_location) {
        auto result_type = IntegerType::of_bool();
        if (outcome == EmitOutcome::TYPE) return Value(result_type);

        LLVMBasicBlockRef alt_blocks[2] = {
            LLVMGetInsertBlock(builder),
            append_block(""),
        };
        auto merge_block = append_block("");

        LLVMValueRef alt_values[2];
        auto condition_value = convert_to_type(left_value, IntegerType::of_bool(), ConvKind::IMPLICIT, left_location);
        alt_values[0] = get_rvalue(condition_value);

        LLVMBasicBlockRef then_block = expr->op == TOK_AND_OP ? alt_blocks[1] : merge_block;
        LLVMBasicBlockRef else_block = expr->op == TOK_AND_OP ? merge_block : alt_blocks[1];
        LLVMBuildCondBr(builder, alt_values[0], then_block, else_block);

        LLVMPositionBuilderAtEnd(builder, alt_blocks[1]);
        alt_values[1] = get_rvalue(convert_to_type(expr->right, result_type, ConvKind::IMPLICIT));
        LLVMBuildBr(builder, merge_block);

        LLVMPositionBuilderAtEnd(builder, merge_block);
        Value result;
        auto phi_value = LLVMBuildPhi(builder, result_type->llvm_type(), "");
        LLVMAddIncoming(phi_value, alt_values, alt_blocks, 2);
        return Value(result_type, phi_value);
    }

    const Type* promote_integer(const Type* type) {
        auto int_type = IntegerType::default_type();

        while (auto enum_type = type_cast<EnumType>(type)) {
            type = enum_type->base_type;
        }

        if (auto type_as_int = type_cast<IntegerType>(type)) {
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

        auto left_as_float = type_cast<FloatingPointType>(left);
        auto right_as_float = type_cast<FloatingPointType>(right);
        if (left_as_float) {
            if (right_as_float) {
                return FloatingPointType::of(max(left_as_float->size, right_as_float->size));
            }
            return left_as_float;
        }
        if (right_as_float) return right_as_float;

        auto left_as_int = type_cast<IntegerType>(left);
        auto right_as_int = type_cast<IntegerType>(right);

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

        left_value = convert_to_type(left_value, convert_type, ConvKind::IMPLICIT, left_location);
        right_value = convert_to_type(right_value, convert_type, ConvKind::IMPLICIT, right_location);

        if (auto as_int = type_cast<IntegerType>(convert_type)) {
            switch (expr->op) {
              default:
                assert(false); // TODO
                break;
              case '=':
                return right_value;
              case '+':
              case TOK_ADD_ASSIGN:
                return Value(result_type, LLVMBuildAdd(builder, get_rvalue(left_value), get_rvalue(right_value), ""));
              case '-':
              case TOK_SUB_ASSIGN:
                return Value(result_type, LLVMBuildSub(builder, get_rvalue(left_value), get_rvalue(right_value), ""));
              case '*':
              case TOK_MUL_ASSIGN:
                return Value(result_type, LLVMBuildMul(builder, get_rvalue(left_value), get_rvalue(right_value), ""));
              case '/':
              case TOK_DIV_ASSIGN:
                if (as_int->is_signed()) {
                    return Value(result_type, LLVMBuildSDiv(builder, get_rvalue(left_value), get_rvalue(right_value), ""));
                } else {
                    return Value(result_type, LLVMBuildUDiv(builder, get_rvalue(left_value), get_rvalue(right_value), ""));
                }
              case '&':
              case TOK_AND_ASSIGN:
                return Value(result_type, LLVMBuildAnd(builder, get_rvalue(left_value), get_rvalue(right_value), ""));
              case '|':
              case TOK_OR_ASSIGN:
                return Value(result_type, LLVMBuildOr(builder, get_rvalue(left_value), get_rvalue(right_value), ""));
              case '^':
              case TOK_XOR_ASSIGN:
                return Value(result_type, LLVMBuildXor(builder, get_rvalue(left_value), get_rvalue(right_value), ""));
              case TOK_LEFT_OP:
              case TOK_LEFT_ASSIGN:
                return Value(result_type, LLVMBuildShl(builder, get_rvalue(left_value), get_rvalue(right_value), ""));
              case TOK_RIGHT_OP:
              case TOK_RIGHT_ASSIGN:
                if (as_int->is_signed()) {
                  return Value(result_type, LLVMBuildAShr(builder, get_rvalue(left_value), get_rvalue(right_value), ""));
                } else {
                  return Value(result_type, LLVMBuildLShr(builder, get_rvalue(left_value), get_rvalue(right_value), ""));
                }
              case '<':       
              case '>':       
              case TOK_LE_OP: 
              case TOK_GE_OP: 
              case TOK_EQ_OP: 
              case TOK_NE_OP:
                return Value(result_type, LLVMBuildICmp(builder,
                                                        llvm_int_predicate(as_int->is_signed(), expr->op),
                                                        get_rvalue(left_value), get_rvalue(right_value),
                                                        ""));
            }
        }

        if (auto as_float = type_cast<FloatingPointType>(convert_type)) {
            switch (expr->op) {
              default:
                assert(false); // TODO
              case '=':
                return right_value;
              case '+':
              case TOK_ADD_ASSIGN:
                return Value(result_type, LLVMBuildFAdd(builder, get_rvalue(left_value), get_rvalue(right_value), ""));
              case '-':
              case TOK_SUB_ASSIGN:
                return Value(result_type, LLVMBuildFSub(builder, get_rvalue(left_value), get_rvalue(right_value), ""));
              case '*':
              case TOK_MUL_ASSIGN:
                return Value(result_type, LLVMBuildFMul(builder, get_rvalue(left_value), get_rvalue(right_value), ""));
              case '/':
              case TOK_DIV_ASSIGN:
                return Value(result_type, LLVMBuildFDiv(builder, get_rvalue(left_value), get_rvalue(right_value), ""));
              case '<':       
              case '>':       
              case TOK_LE_OP: 
              case TOK_GE_OP: 
              case TOK_EQ_OP: 
              case TOK_NE_OP:
                return Value(result_type, LLVMBuildFCmp(builder,
                                                        llvm_float_predicate(expr->op),
                                                        get_rvalue(left_value), get_rvalue(right_value),
                                                        ""));
            }
        }

        return Value();
    }

    Value emit_pointer_binary_operation(BinaryExpr* expr, Value left_value, Value right_value, Location left_location, Location right_location) {
        auto op = expr->op;
        auto op_flags = operator_flags(op);

        auto llvm_target_data = TranslationUnitContext::it->llvm_target_data;
        auto left_pointer_type = type_cast<PointerType>(left_value.type);
        auto right_pointer_type = type_cast<PointerType>(right_value.type);

        if (left_pointer_type && right_pointer_type) {
            if (op == '-' && left_pointer_type->base_type->unqualified() == right_pointer_type->base_type->unqualified()) {
                auto result_type = IntegerType::of_size(IntegerSignedness::SIGNED);
                if (outcome == EmitOutcome::TYPE) return Value(result_type);
            
                auto left_int = LLVMBuildPtrToInt(builder, get_rvalue(left_value), result_type->llvm_type(), "");
                auto right_int = LLVMBuildPtrToInt(builder, get_rvalue(right_value), result_type->llvm_type(), "");
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
                                                    get_rvalue(left_value), get_rvalue(right_value),
                                                    ""));
        }

        if (op == '+'|| op == TOK_ADD_ASSIGN || op == '-' || op == TOK_SUB_ASSIGN) {
            right_value = convert_to_type(right_value, promote_integer(right_value.type), ConvKind::IMPLICIT, right_location);
            if (!type_cast<IntegerType>(right_value.type)) return Value();

            if (outcome == EmitOutcome::TYPE) return Value(left_pointer_type);

            LLVMValueRef index = get_rvalue(right_value);

            if (op == '-' || op == TOK_SUB_ASSIGN) {
                index = LLVMBuildNeg(builder, index, "");
            }

            return Value(left_pointer_type, LLVMBuildGEP2(builder, left_pointer_type->base_type->llvm_type(), get_rvalue(left_value), &index, 1, ""));
        }

        return Value();
    }

    Value convert_array_to_pointer(const Value& value) {
        auto zero = TranslationUnitContext::it->zero_size;

        if (auto array_type = type_cast<ArrayType>(value.type)) {
            auto result_type = array_type->element_type->pointer_to();
            if (outcome == EmitOutcome::TYPE) return Value(result_type);

            LLVMValueRef indices[2] = {zero, zero};
            return Value(result_type,
                         LLVMBuildGEP2(builder, array_type->llvm_type(), get_lvalue(value), indices, 2, ""));
        }

        return value;
    }

    virtual VisitExpressionOutput visit(BinaryExpr* expr) override {
        auto op = expr->op;
        Value intermediate;
        Value left_value = accept_expr(expr->left).value.unqualified();

        if (op == TOK_AND_OP || op == TOK_OR_OP) {
            intermediate = emit_logical_binary_operation(expr, left_value, expr->left->location);
        } else {
            left_value = convert_array_to_pointer(left_value);
            auto left_pointer_type = type_cast<PointerType>(left_value.type);

            auto right_value = accept_expr(expr->right).value.unqualified();
            right_value = convert_array_to_pointer(right_value);
            auto right_pointer_type = type_cast<PointerType>(right_value.type);

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
            store_value(left_value, intermediate, expr->location);
        }

        return VisitExpressionOutput(intermediate);
    }

    virtual VisitExpressionOutput visit(CastExpr* expr) override {
        if (outcome == EmitOutcome::TYPE) return VisitExpressionOutput(expr->type);

        auto value = convert_to_type(expr->expr, expr->type, ConvKind::EXPLICIT);
        return VisitExpressionOutput(value);
    }

    virtual VisitExpressionOutput visit(ConditionExpr* expr) override {
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

        LLVMBasicBlockRef alt_blocks[2] = {
            append_block("then"),
            append_block("else"),
        };
        auto merge_block = append_block("merge");

        auto condition_value = convert_to_type(expr->condition, IntegerType::of(IntegerSignedness::UNSIGNED, IntegerSize::BOOL), ConvKind::IMPLICIT);
        LLVMBuildCondBr(builder, get_rvalue(condition_value), alt_blocks[0], alt_blocks[1]);

        LLVMValueRef alt_values[2];

        LLVMPositionBuilderAtEnd(builder, alt_blocks[0]);
        auto then_value = convert_to_type(expr->then_expr, result_type, ConvKind::IMPLICIT);
        if (result_type != &VoidType::it) alt_values[0] = get_rvalue(then_value);
        LLVMBuildBr(builder, merge_block);

        LLVMPositionBuilderAtEnd(builder, alt_blocks[1]);
        auto else_value = convert_to_type(expr->else_expr, result_type, ConvKind::IMPLICIT);
        if (result_type != &VoidType::it) alt_values[1] = get_rvalue(else_value);
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

        auto value = accept_expr(expr->expr).value.unqualified();
        auto pointer_type = type_cast<PointerType>(value.type);
        auto result_type = pointer_type->base_type;
        if (outcome == EmitOutcome::TYPE) return VisitExpressionOutput(result_type);

        return VisitExpressionOutput(Value(ValueKind::LVALUE, result_type, get_rvalue(value)));
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
            while (auto enum_type = type_cast<EnumType>(type->unqualified())) {
                type = enum_type->base_type;
            }

            auto int_type = type_cast<IntegerType>(type);
            return VisitExpressionOutput(Value::of_int(int_type, enum_constant->value).bit_cast(result_type));

        }

        if (auto variable = declarator->variable()) {
            if (outcome == EmitOutcome::IR && this_value.is_valid() && !variable->value.is_valid()) {
                if (auto this_type = type_cast<StructuredType>(this_value.type->unqualified())) {
                    assert(variable->member);

                    if (declarator->scope != this_type->scope) {
                        message(Severity::ERROR, expr->location) << "'" << *declarator->identifier << "' is not a member of the immediately enclosing type so is inaccessible via 'this'\n";
                        declarator->message_see_declaration();
                        pause_messages();
                    } else {
                        return VisitExpressionOutput(Value(ValueKind::LVALUE, declarator->type,
                            LLVMBuildGEP2(builder, this_type->llvm_type(), get_lvalue(this_value), variable->member->gep_indices.data(), variable->member->gep_indices.size(), "")));
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
        auto lvalue = accept_expr(expr->expr).value.unqualified();
        auto before_value = Value(lvalue.type, get_rvalue(lvalue));

        const Type* result_type = before_value.type;
        if (outcome == EmitOutcome::TYPE) return VisitExpressionOutput(result_type);

        LLVMValueRef one = LLVMConstInt(result_type->llvm_type(), 1, false);

        Value after_value;
        switch (expr->op) {
          case TOK_INC_OP:
            after_value = Value(result_type, LLVMBuildAdd(builder, get_rvalue(before_value), one, ""));
            break;
          case TOK_DEC_OP:
            after_value = Value(result_type, LLVMBuildSub(builder, get_rvalue(before_value), one, ""));
            break;
        }

        store_value(lvalue, after_value, expr->location);
        return VisitExpressionOutput(expr->postfix ? before_value : after_value);
    }

    Value emit_object_of_member_expr(MemberExpr* expr) {
        auto object = accept_expr(expr->object).value;
        if (auto pointer_type = type_cast<PointerType>(object.type)) {
            object = Value(ValueKind::LVALUE, pointer_type->base_type, get_rvalue(object));
        }
        return object;
    }

    virtual VisitExpressionOutput visit(MemberExpr* expr) override {
        if (!expr->member) return VisitExpressionOutput(Value::of_zero_int());

        auto object_type = get_expr_type(expr->object)->unqualified();

        if (auto pointer_type = type_cast<PointerType>(object_type)) {
            object_type = pointer_type->base_type->unqualified();
        }

        if (auto struct_type = type_cast<StructuredType>(object_type)) {
            VisitExpressionOutput output;

            auto result_type = expr->member->type;
            if (outcome == EmitOutcome::TYPE) return VisitExpressionOutput(result_type);

            if (auto member_variable = expr->member->variable()) {
                auto object = emit_object_of_member_expr(expr);
                
                auto llvm_struct_type = struct_type->llvm_type();

                output.value = Value(ValueKind::LVALUE, result_type, LLVMBuildInBoundsGEP2(builder, llvm_struct_type, get_lvalue(object),
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
        auto value = accept_expr(expr->expr).value.unqualified();
        if (outcome == EmitOutcome::TYPE) return VisitExpressionOutput(value.type);

        return VisitExpressionOutput(value.type, get_rvalue(value));
    }

    virtual VisitExpressionOutput visit(CallExpr* expr) override {
        if (outcome == EmitOutcome::FOLD) {
            message(Severity::ERROR, expr->location) << "cannot call function in constant expression\n";
            throw FoldError(true);
        }

        auto function_output = accept_expr(expr->function);
        auto function_value = function_output.value.unqualified();

        if (auto pointer_type = type_cast<PointerType>(function_value.type)) {
            function_value = Value(ValueKind::LVALUE, pointer_type->base_type, get_rvalue(function_value));
        }

        if (auto function_type = type_cast<FunctionType>(function_value.type)) {
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

                if (object_value.kind != ValueKind::LVALUE) {
                    auto lvalue = allocate_auto_storage(object_value.type, "");
                    lvalue.store(builder, object_value);
                    object_value = lvalue;
                }

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
                if (pass_by_ref_type = type_cast<PassByReferenceType>(expected_type)) {
                    expected_type = pass_by_ref_type->base_type;
                }

                Location param_location;
                if (member_expr && i == 0) {
                    param_value = object_value;
                    param_location = member_expr->object->location;
                } else {
                    auto param_expr = expr->parameters[param_expr_idx++];
                    param_value = accept_expr(param_expr).value;
                    param_location = param_expr->location;
                }

                if (pass_by_ref_type) {
                    if (param_value.kind != ValueKind::LVALUE && (expected_type->qualifiers() & QUALIFIER_CONST)) {
                        param_value = convert_to_type(param_value.unqualified(), expected_type, ConvKind::IMPLICIT, member_expr->location);
                        auto lvalue = allocate_auto_storage(param_value.type, "");
                        lvalue.store(builder, param_value);
                        llvm_params[i] = get_lvalue(lvalue);
                    } else if (param_value.kind != ValueKind::LVALUE) {
                        message(Severity::ERROR, param_location) << "rvalue type '" << PrintType(param_value.type) << "' incompatible with non-const pass-by-reference parameter type '"
                                                                 << PrintType(function_type->parameter_types[i]) << "'\n";
                        llvm_params[i] = LLVMConstNull(pass_by_ref_type->llvm_type());
                    } else {
                        if (param_value.type->unqualified() != expected_type->unqualified()) {
                            message(Severity::ERROR, param_location) << "lvalue type '" << PrintType(param_value.type) << "' incompatible with pass-by-reference parameter type '"
                                                                     << PrintType(function_type->parameter_types[i]) << "'\n";
                        }

                        if (param_value.qualifiers > expected_type->qualifiers()) {
                            message(Severity::ERROR, param_location) << "lvalue type '" << PrintType(param_value.type) << "' has more type qualifiers than pass-by-reference parameter type '"
                                                                     << PrintType(function_type->parameter_types[i]) << "'\n";
                        }

                        param_value = param_value.unqualified();
                        llvm_params[i] = get_lvalue(param_value);
                    }
                } else {
                    param_value = convert_to_type(param_value.unqualified(), expected_type, ConvKind::IMPLICIT, member_expr->location);
                    llvm_params[i] = get_rvalue(param_value);
                }
            }

            auto result = LLVMBuildCall2(builder, function_type->llvm_type(), get_lvalue(function_value), llvm_params.data(), llvm_params.size(), "");
            return VisitExpressionOutput(result_type, result);
        }

        message(Severity::ERROR, expr->location) << "called object type '" << PrintType(function_value.type) << "' is not a function or function pointer\n";

        return VisitExpressionOutput();
    }

    virtual VisitExpressionOutput visit(SizeOfExpr* expr) override {
        auto zero = TranslationUnitContext::it->zero_size;
        auto llvm_target_data = TranslationUnitContext::it->llvm_target_data;

        auto result_type = IntegerType::of_size(IntegerSignedness::UNSIGNED);

        if (expr->type->partition() == TypePartition::INCOMPLETE) {
            return VisitExpressionOutput(result_type, zero);
        }

        if (outcome == EmitOutcome::TYPE) return VisitExpressionOutput(result_type);

        return VisitExpressionOutput(result_type, size_const_int(LLVMStoreSizeOfType(llvm_target_data, expr->type->llvm_type())));
    }

    virtual VisitExpressionOutput visit(SubscriptExpr* expr) override {
        auto zero = TranslationUnitContext::it->zero_size;

        auto left_value = accept_expr(expr->left).value.unqualified();
        auto index_value = accept_expr(expr->right).value.unqualified();

        if (type_cast<IntegerType>(left_value.type)) {
            swap(left_value, index_value);
        }

        if (auto pointer_type = type_cast<PointerType>(left_value.type)) {
            auto result_type = pointer_type->base_type;
            if (outcome == EmitOutcome::TYPE) return VisitExpressionOutput(result_type);

            LLVMValueRef index = get_rvalue(index_value);
            return VisitExpressionOutput(Value(
                ValueKind::LVALUE,
                result_type,
                LLVMBuildGEP2(builder, result_type->llvm_type(), get_rvalue(left_value), &index, 1, "")));
        }

        if (auto array_type = type_cast<ArrayType>(left_value.type)) {
            auto result_type = array_type->element_type;
            if (outcome == EmitOutcome::TYPE) return VisitExpressionOutput(result_type);

            LLVMValueRef indices[2] = { zero, get_rvalue(index_value) };
            return VisitExpressionOutput(Value(
                ValueKind::LVALUE,
                result_type,
                LLVMBuildGEP2(builder, array_type->llvm_type(), get_lvalue(left_value), indices, 2, "")));
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

Construct::Construct(Emitter* emitter): emitter(emitter) {
    parent = emitter->innermost_construct;
    emitter->innermost_construct = this;
}

Construct::~Construct() {
    emitter->innermost_construct = parent;
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
    Emitter emitter(EmitOutcome::TYPE, options);
    return emitter.accept_expr(const_cast<Expr*>(expr)).value.type;
}

Value fold_expr(const Expr* expr) {
    static const EmitOptions options;
    Emitter emitter(EmitOutcome::FOLD, options);
    try {
        return emitter.accept_expr(const_cast<Expr*>(expr)).value;
    } catch (FoldError&) {
        return Value::of_recover(get_expr_type(expr));
    }
}

void Module::emit_pass(const EmitOptions& options) {
    Emitter emitter(EmitOutcome::IR, options);
    emitter.module = this;

    emitter.accept_scope(file_scope);
    for (auto scope: type_scopes) {
        emitter.accept_scope(scope);
    }

    LLVMVerifyModule(llvm_module, LLVMPrintMessageAction, nullptr);
}
