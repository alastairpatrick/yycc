#include "Emitter.h"
#include "lex/StringLiteral.h"
#include "LLVM.h"
#include "Message.h"
#include "parse/AssocPrec.h"
#include "parse/Declaration.h"
#include "TranslationUnitContext.h"
#include "Visitor.h"

struct Emitter;

enum class EmitOutcome {
    TYPE,
    FOLD,
    IR,
};

struct EmitError {
    explicit EmitError(bool error_reported): error_reported(error_reported) {}
    bool error_reported;
};

struct Module {
    LLVMModuleRef llvm_module{};

    // Maps from a constant to a constant global initialized with that constant. Intended only to pool strings.
    // Note that LLVM internally performs constant uniqueing, ensuring that constants with the same type and
    // value are the same instance.
    unordered_map<LLVMValueRef, LLVMValueRef> reified_constants;
};

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

struct Emitter: Visitor {
    EmitOutcome outcome;
    const EmitOptions& options;

    Module* module{};
    Emitter* parent{};

    Declarator* function_declarator{};
    const FunctionType* function_type{};
    LLVMBuilderRef builder{};
    LLVMBuilderRef temp_builder{};
    LLVMValueRef function{};
    LLVMBasicBlockRef entry_block{};
    LLVMBasicBlockRef unreachable_block{};
    unordered_map<InternedString, LLVMBasicBlockRef> goto_labels;
    Construct* innermost_construct{};
    SwitchConstruct* innermost_switch{};

    Emitter(EmitOutcome outcome, const EmitOptions& options): outcome(outcome), options(options) {
        auto llvm_context = TranslationUnitContext::it->llvm_context;
        if (outcome != EmitOutcome::TYPE) {
            builder = LLVMCreateBuilderInContext(llvm_context);
        }
        if (outcome == EmitOutcome::IR) {
            temp_builder = LLVMCreateBuilderInContext(llvm_context);
        }
    }

    ~Emitter() {
        if (builder) LLVMDisposeBuilder(builder);
        if (temp_builder) LLVMDisposeBuilder(temp_builder);
    }

    void emit(ASTNode* node) {
        if (auto declaration = dynamic_cast<Declaration*>(node)) {
            emit(declaration);
        } else if (auto statement = dynamic_cast<Statement*>(node)) {
            emit(statement);
        }
    }

    void emit(Declaration* declaration) {
        for (auto declarator: declaration->declarators) {
            emit(declarator);
        }
    }

    void emit(Scope* scope) {
        for (auto declarator: scope->declarators) {
            emit(declarator);
        }
    }

    void emit(Declarator* declarator) {
        declarator = declarator->primary;
        if (declarator->status >= DeclaratorStatus::EMITTED) return;
        assert(declarator->status == DeclaratorStatus::RESOLVED);

        declarator->accept(*this, VisitDeclaratorInput());

        declarator->status = DeclaratorStatus::EMITTED;
    }
    
    LLVMValueRef get_rvalue(const Value &value) {
        if (outcome == EmitOutcome::IR) {
            return value.get_rvalue(builder);
        } else {
            if (value.is_const()) {
                return value.get_const();
            } else {
                throw EmitError(false);
            }
        }
    }

    void store_value(const Value& lvalue, const Value& rvalue, const Location& location) {
        if (outcome == EmitOutcome::IR) {
            if (lvalue.kind == ValueKind::LVALUE) {
                lvalue.store(builder, rvalue);
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

    Value emit(Statement* statement) {
        return accept(statement, VisitStatementInput()).value;
    }

    virtual void pre_visit(Statement* statement) override {
        if (!statement->labels.size()) return;

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
    }

    template <typename T, typename U>
    const T* type_cast(const U* type) {
        assert(type->qualifiers() == 0);
        return dynamic_cast<const T*>(type);
    }

    Value convert_to_type(Expr* expr, const Type* dest_type, ConvKind kind) {
        if (expr->is_null_literal() && type_cast<PointerType>(dest_type->unqualified())) {
            return Value(dest_type, LLVMConstNull(dest_type->llvm_type()));
        }

        auto value = emit(expr).unqualified();
        return convert_to_type(value, dest_type, kind, expr->location);
    }

    Value convert_to_type(Value value, const Type* dest_type, ConvKind kind, const Location& location) {
        assert(value.type->qualifiers() == 0);
        
        dest_type = dest_type->unqualified();

        if (value.type == dest_type) return value;

        const EnumType* dest_enum_type{};
        if (dest_enum_type = type_cast<EnumType>(dest_type)) {
            dest_type = dest_enum_type->base_type;
        }

        VisitTypeInput input;
        input.value = value;
        input.dest_type = dest_type;
        auto output = value.type->accept(*this, input);
        auto result = output.value;

        if (dest_enum_type) {
            result = result.bit_cast(dest_enum_type);
            dest_type = dest_enum_type;
        }

        if (!result.is_valid()) {
            message(Severity::ERROR, location) << "cannot convert from type '" << PrintType(value.type)
                                               << "' to type '" << PrintType(dest_type) << "'\n";
            pause_messages();
            if (dest_type != &VoidType::it) {
                result = Value(dest_type, LLVMConstNull(dest_type->llvm_type()));
            } else {
                return Value::of_zero_int();
            }
        } else if ((dest_enum_type || output.conv_kind != ConvKind::IMPLICIT) && kind == ConvKind::IMPLICIT) {
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
        function = entity->value.get_lvalue();

        entry_block = append_block("");
        unreachable_block = append_block("");
        LLVMPositionBuilderAtEnd(builder, entry_block);

        for (size_t i = 0; i < entity->parameters.size(); ++i) {
            auto param = entity->parameters[i];
            auto param_entity = param->entity();

            auto storage = LLVMBuildAlloca(builder, param->type->llvm_type(), c_str(param->identifier));
            param_entity->value = Value(ValueKind::LVALUE, param->type, storage);
            LLVMBuildStore(builder, LLVMGetParam(function, i), storage);
        }

        emit(entity->body);

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

    void emit_auto_initializer(Value dest, Expr* expr) {
        auto context = TranslationUnitContext::it;

        if (auto uninitializer = dynamic_cast<UninitializedExpr*>(expr)) {
            return;
        }

        Value scalar_value;
        if (auto initializer = dynamic_cast<InitializerExpr*>(expr)) {
            if (auto array_type = type_cast<ResolvedArrayType>(dest.type)) {
                for (size_t i = 0; i < array_type->size; ++i) {
                    LLVMValueRef indices[] = { context->zero_size, size_const_int(i) };
                    LLVMValueRef dest_element = LLVMBuildGEP2(builder, array_type->llvm_type(), dest.get_lvalue(), indices, 2, "");
                    emit_auto_initializer(Value(ValueKind::LVALUE, array_type->element_type, dest_element), initializer->elements[i]);
                }
                return;
            }

            if (auto struct_type = type_cast<StructType>(dest.type)) {
                size_t initializer_idx{};
                for (auto declaration: struct_type->declarations) {
                    for (auto member: declaration->declarators) {
                        if (auto member_variable = member->variable()) {
                            LLVMValueRef dest_element = LLVMBuildInBoundsGEP2(builder, struct_type->llvm_type(), dest.get_lvalue(),
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

        LLVMBuildStore(builder, get_rvalue(scalar_value), dest.get_lvalue());
    }
    
    Value emit_static_initializer(const Type* dest_type, Expr* expr) {
        if (auto uninitializer = dynamic_cast<UninitializedExpr*>(expr)) {
            return Value(dest_type, LLVMGetUndef(dest_type->llvm_type()));
        }

        if (auto initializer = dynamic_cast<InitializerExpr*>(expr)) {
            if (auto array_type = type_cast<ResolvedArrayType>(dest_type)) {
                vector<LLVMValueRef> values(array_type->size);
                for (size_t i = 0; i < array_type->size; ++i) {
                    values[i] = emit_static_initializer(array_type->element_type, initializer->elements[i]).get_const();
                }

                return Value(array_type, LLVMConstArray(array_type->element_type->llvm_type(), values.data(), values.size()));
            }

            if (auto struct_type = type_cast<StructType>(dest_type)) {
                size_t initializer_idx{};
                vector<LLVMValueRef> values;
                for (auto declaration: struct_type->declarations) {
                    for (auto member: declaration->declarators) {
                        if (auto member_variable = member->variable()) {
                            values.push_back(emit_static_initializer(member->type, initializer->elements[initializer_idx++]).get_const());
                        }
                    }
                }

                return Value(struct_type, LLVMConstNamedStruct(struct_type->llvm_type(), values.data(), values.size()));
            }

            return emit_scalar_initializer(dest_type, initializer);
        }

        return convert_to_type(expr, dest_type, ConvKind::IMPLICIT);
    }

    void emit_variable(Declarator* declarator, Variable* entity) {
        auto type = declarator->primary->type;
        auto llvm_type = type->llvm_type();

        auto null_value = LLVMConstNull(llvm_type);

        if (entity->storage_duration == StorageDuration::AUTO) {
            auto first_insn = LLVMGetFirstInstruction(entry_block);
            if (first_insn) {
                LLVMPositionBuilderBefore(temp_builder, first_insn);
            } else {
                LLVMPositionBuilderAtEnd(temp_builder, entry_block);
            }
            auto storage = LLVMBuildAlloca(temp_builder, llvm_type, c_str(declarator->identifier));

            entity->value = Value(ValueKind::LVALUE, type, storage);

            if (entity->initializer) {
                emit_auto_initializer(entity->value, entity->initializer);
            } else if (options.initialize_variables) {
                LLVMBuildStore(builder, null_value, storage);
            }

        } else if (entity->storage_duration == StorageDuration::STATIC) {
            auto global = entity->value.get_lvalue();

            LLVMValueRef initial = null_value;
            if (entity->initializer) {
                initial = emit_static_initializer(type, entity->initializer).get_const();
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

    ConvKind check_pointer_conversion(const Type* source_base_type, const Type* dest_base_type) {
        auto unqualified_source_base_type = source_base_type->unqualified();
        auto unqualified_dest_base_type = dest_base_type->unqualified();

        ConvKind result = unqualified_source_base_type == unqualified_dest_base_type ? ConvKind::IMPLICIT : ConvKind::C_IMPLICIT;

        if (unqualified_dest_base_type == &VoidType::it && source_base_type->partition() != TypePartition::FUNCTION) {
            result = ConvKind::IMPLICIT;
        }

        if (result == ConvKind::IMPLICIT) {
            if (auto source_base_pointer_type = type_cast<PointerType>(source_base_type->unqualified())) {
                if (auto dest_base_pointer_type = type_cast<PointerType>(dest_base_type->unqualified())) {
                    result = check_pointer_conversion(source_base_pointer_type, dest_base_pointer_type);
                }
            }
        }

        if ((result == ConvKind::IMPLICIT) && (dest_base_type->qualifiers() < source_base_type->qualifiers())) {
            result = ConvKind::C_IMPLICIT;
        }

        if (source_base_type->partition() != dest_base_type->partition()) {
            if (source_base_type->partition() == TypePartition::FUNCTION) result = ConvKind::EXPLICIT;
            if (dest_base_type->partition() == TypePartition::FUNCTION) result = ConvKind::EXPLICIT;
        }

        return result;
    }

    VisitTypeOutput visit(const ResolvedArrayType* source_type, const VisitTypeInput& input) {
        auto dest_type = input.dest_type;
        auto value = input.value;

        if (auto dest_array_type = type_cast<ResolvedArrayType>(dest_type)) {
            if (value.is_const() && source_type->element_type == dest_array_type->element_type && source_type->size <= dest_array_type->size) {
                LLVMValueRef source_array = value.get_const();
                vector<LLVMValueRef> resized_array_values(dest_array_type->size);
                size_t i;
                for (i = 0; i < source_type->size; ++i) {
                    resized_array_values[i] = LLVMGetAggregateElement(source_array, i);
                }
                LLVMValueRef null_value = LLVMConstNull(source_type->element_type->llvm_type());
                for (; i < dest_array_type->size; ++i) {
                    resized_array_values[i] = null_value;
                }

                // TODO: LLVMConstArray2
                LLVMValueRef resized_array = LLVMConstArray(source_type->element_type->llvm_type(), resized_array_values.data(), resized_array_values.size());
                return VisitTypeOutput(dest_type, resized_array);
            }
        }

        if (auto pointer_type = type_cast<PointerType>(dest_type)) {
            if (value.kind == ValueKind::LVALUE) {
                return VisitTypeOutput(dest_type, value.get_lvalue(), check_pointer_conversion(source_type->element_type, pointer_type->base_type));
            }

            if (value.is_const()) {
                auto& global = module->reified_constants[value.get_const()];
                if (!global) {
                    global = LLVMAddGlobal(module->llvm_module, value.type->llvm_type(), "const");
                    LLVMSetGlobalConstant(global, true);
                    LLVMSetLinkage(global, LLVMPrivateLinkage);
                    LLVMSetInitializer(global, value.get_const());
                }
                return VisitTypeOutput(dest_type, global);
            }
        }

        return VisitTypeOutput(); 
    }

    virtual VisitTypeOutput visit(const EnumType* source_type, const VisitTypeInput& input) override {
        return source_type->base_type->accept(*this, input);
    }

    virtual VisitTypeOutput visit(const FloatingPointType* source_type, const VisitTypeInput& input) override {
        auto dest_type = input.dest_type;
        auto value = input.value;

        if (type_cast<FloatingPointType>(dest_type)) {
            return VisitTypeOutput(dest_type, LLVMBuildFPCast(builder, get_rvalue(value), input.dest_type->llvm_type(), ""));
        }

        if (auto dest_int_type = type_cast<IntegerType>(dest_type)) {
            if (dest_int_type->is_signed()) {
                return VisitTypeOutput(dest_type, LLVMBuildFPToSI(builder, get_rvalue(value), input.dest_type->llvm_type(), ""));
            } else {
                return VisitTypeOutput(dest_type, LLVMBuildFPToUI(builder, get_rvalue(value), input.dest_type->llvm_type(), ""));
            }
        }

        return VisitTypeOutput();
    }

    virtual VisitTypeOutput visit(const FunctionType* source_type, const VisitTypeInput& input) override {
        auto dest_type = input.dest_type;
        auto value = input.value;

        if (auto pointer_type = type_cast<PointerType>(dest_type)) {
            return VisitTypeOutput(dest_type, value.get_lvalue());
        }

        return VisitTypeOutput();
    }

    virtual VisitTypeOutput visit(const IntegerType* source_type, const VisitTypeInput& input) override {
        auto dest_type = input.dest_type;
        auto value = input.value;

        if (auto int_target = type_cast<IntegerType>(dest_type)) {
            return VisitTypeOutput(dest_type, LLVMBuildIntCast2(builder, get_rvalue(value), dest_type->llvm_type(), source_type->is_signed(), ""));
        }

        if (type_cast<FloatingPointType>(dest_type)) {
            if (source_type->is_signed()) {
                return VisitTypeOutput(dest_type, LLVMBuildSIToFP(builder, get_rvalue(value), dest_type->llvm_type(), ""));
            } else {
                return VisitTypeOutput(dest_type, LLVMBuildUIToFP(builder, get_rvalue(value), dest_type->llvm_type(), ""));
            }
        }

        if (type_cast<PointerType>(dest_type)) {
            return VisitTypeOutput(dest_type, LLVMBuildIntToPtr(builder, get_rvalue(value), dest_type->llvm_type(), ""), ConvKind::EXPLICIT);
        }

        return VisitTypeOutput();
    }
    
    VisitTypeOutput visit(const PointerType* source_type, const VisitTypeInput& input) {
        auto dest_type = input.dest_type;
        auto value = input.value;

        if (auto dest_pointer_type = type_cast<PointerType>(dest_type)) {
            return VisitTypeOutput(value.bit_cast(dest_type), check_pointer_conversion(source_type->base_type, dest_pointer_type->base_type));
        }

        if (type_cast<IntegerType>(dest_type)) {
            return VisitTypeOutput(dest_type, LLVMBuildPtrToInt(builder, get_rvalue(value), dest_type->llvm_type(), ""));
        }

        return VisitTypeOutput();
    }

    VisitTypeOutput visit(const VoidType* source_type, const VisitTypeInput& input) {
        return VisitTypeOutput();
    }

    virtual VisitStatementOutput visit(CompoundStatement* statement, const VisitStatementInput& input) override {
        for (auto node: statement->nodes) {
            emit(node);
        }
        return VisitStatementOutput();
    };

    VisitStatementOutput visit(ForStatement* statement, const VisitStatementInput& input) {
        Construct construct(this);

        if (statement->declaration) {
            emit(statement->declaration);
        }

        if (statement->initialize) {
            emit(statement->initialize);
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
        emit(statement->body);

        if (statement->iterate) {
            LLVMBuildBr(builder, iterate_block);

            LLVMPositionBuilderAtEnd(builder, iterate_block);
            emit(statement->iterate);
        }

        LLVMBuildBr(builder, loop_block);
        LLVMPositionBuilderAtEnd(builder, end_block);

        return VisitStatementOutput();
    }

    VisitStatementOutput visit(GoToStatement* statement, const VisitStatementInput& input) {
        
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

    VisitStatementOutput visit(IfElseStatement* statement, const VisitStatementInput& input) {
        auto then_block = append_block("if_c");
        LLVMBasicBlockRef else_block;
        if (statement->else_statement) else_block = append_block("if_a");
        auto end_block = append_block("if_e");
        if (!statement->else_statement) else_block = end_block;

        auto condition_value = convert_to_type(statement->condition, IntegerType::of_bool(), ConvKind::IMPLICIT);
        LLVMBuildCondBr(builder, get_rvalue(condition_value), then_block, else_block);

        LLVMPositionBuilderAtEnd(builder, then_block);
        emit(statement->then_statement);
        LLVMBuildBr(builder, end_block);

        if (statement->else_statement) {
            LLVMPositionBuilderAtEnd(builder, else_block);
            emit(statement->else_statement);
            LLVMBuildBr(builder, end_block);
        }

        LLVMPositionBuilderAtEnd(builder, end_block);

        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(ReturnStatement* statement, const VisitStatementInput& input) override {
        if (function_type->return_type->unqualified() == &VoidType::it) {
            if (statement->expr) {
                auto type = get_expr_type(statement->expr);
                if (type->unqualified() != &VoidType::it) {
                    message(Severity::ERROR, statement->expr->location) << "void function '" << *function_declarator->identifier << "' should not return a value\n";
                }
            }
            LLVMBuildRetVoid(builder);
        } else {
            Value value;
            if (statement->expr) {
                value = convert_to_type(statement->expr, function_type->return_type->unqualified(), ConvKind::IMPLICIT);
            } else {
                message(Severity::ERROR, statement->location) << "non-void function '" << *function_declarator->identifier << "' should return a value\n";
                value = Value(function_type->return_type, LLVMConstNull(function_type->return_type->llvm_type()));
            }
            LLVMBuildRet(builder, get_rvalue(value));
        }

        LLVMPositionBuilderAtEnd(builder, unreachable_block);

        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(SwitchStatement* statement, const VisitStatementInput& input) override {
        SwitchConstruct construct(this);

        construct.break_block = append_block("");

        LLVMBasicBlockRef default_block{};
        if (statement->num_defaults) {
            construct.default_label = default_block = append_block("");
        } else {
            default_block = construct.break_block;
        }

        auto expr_value = emit(statement->expr).unqualified();
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

        emit(statement->body);

        LLVMBuildBr(builder, construct.break_block);
        LLVMPositionBuilderAtEnd(builder, construct.break_block);

        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(AddressExpr* expr, const VisitStatementInput& input) override {
        auto value = emit(expr->expr);
        auto result_type = value.type->pointer_to();
        if (outcome == EmitOutcome::TYPE) return VisitStatementOutput(result_type);

        return VisitStatementOutput(result_type, value.get_lvalue());
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

    Value emit_regular_binary_operation(BinaryExpr* expr, Value left_value, Value right_value, const Location& left_location, const Location& right_location) {
        bool is_assign = is_assignment_token(expr->op);
        auto convert_type = is_assign ? left_value.type : usual_arithmetic_conversions(left_value.type, right_value.type);
        if (!convert_type) return Value();

        auto result_type = convert_type;
        if (llvm_float_predicate(expr->op)) {
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
              case TOK_LEFT_OP:
                return Value(result_type, LLVMBuildShl(builder, get_rvalue(left_value), get_rvalue(right_value), ""));
              case TOK_RIGHT_OP:
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

    Value emit_pointer_arithmetic_operation(BinaryExpr* expr, const PointerType* pointer_type, Value left_value, Value right_value) {
        auto llvm_target_data = TranslationUnitContext::it->llvm_target_data;

        if (expr->op == '+' || expr->op == TOK_ADD_ASSIGN) {
            if (outcome == EmitOutcome::TYPE) return Value(pointer_type);

            LLVMValueRef index = get_rvalue(right_value);
            return Value(pointer_type, LLVMBuildGEP2(builder, pointer_type->base_type->llvm_type(), get_rvalue(left_value), &index, 1, ""));

        } else if (expr->op == '-') {
            auto result_type = IntegerType::of_size(IntegerSignedness::SIGNED);
            if (outcome == EmitOutcome::TYPE) return Value(result_type);
            
            auto left_int = LLVMBuildPtrToInt(builder, get_rvalue(left_value), result_type->llvm_type(), "");
            auto right_int = LLVMBuildPtrToInt(builder, get_rvalue(right_value), result_type->llvm_type(), "");
            auto byte_diff = LLVMBuildSub(builder, left_int, right_int, "");
            auto size_of_base_type = LLVMStoreSizeOfType(llvm_target_data, pointer_type->base_type->llvm_type());
            auto result = LLVMBuildSDiv(builder, byte_diff, LLVMConstInt(result_type->llvm_type(), size_of_base_type, true), "");
            return Value(result_type, result);
        }

        return Value();
    }

    Value convert_array_to_pointer(Value value) {
        auto zero = TranslationUnitContext::it->zero_size;

        if (auto array_type = type_cast<ArrayType>(value.type)) {
            auto result_type = array_type->element_type->pointer_to();
            if (outcome == EmitOutcome::TYPE) return Value(result_type);

            LLVMValueRef indices[2] = {zero, zero};
            return Value(result_type,
                         LLVMBuildGEP2(builder, array_type->llvm_type(), value.get_lvalue(), indices, 2, ""));
        }

        return value;
    }

    virtual VisitStatementOutput visit(BinaryExpr* expr, const VisitStatementInput& input) override {
        auto left_value = emit(expr->left).unqualified();
        left_value = convert_array_to_pointer(left_value);
        auto left_pointer_type = type_cast<PointerType>(left_value.type);

        auto right_value = emit(expr->right).unqualified();
        right_value = convert_array_to_pointer(right_value);
        auto right_pointer_type = type_cast<PointerType>(right_value.type);

        Value intermediate;
        if (left_pointer_type) {
            intermediate = emit_pointer_arithmetic_operation(expr, left_pointer_type, left_value, right_value);
        } else if (right_pointer_type) {
            intermediate = emit_pointer_arithmetic_operation(expr, right_pointer_type, right_value, left_value);
        } else {
            intermediate = emit_regular_binary_operation(expr, left_value, right_value, expr->left->location, expr->right->location);
        }

        if (!intermediate.is_valid()) {
            auto& stream = message(Severity::ERROR, expr->location) << "'" << expr->message_kind() << "' operation may not be evaluated with operands of types '"
                                                                           << PrintType(left_value.type) << "' and '" << PrintType(right_value.type) << "'\n";
            pause_messages();
            intermediate = Value::of_zero_int();
        }

        if (outcome == EmitOutcome::TYPE) return VisitStatementOutput(intermediate);

        if (is_assignment_token(expr->op)) {
            store_value(left_value, intermediate, expr->location);
        }

        return VisitStatementOutput(intermediate);
    }

    virtual VisitStatementOutput visit(CallExpr* expr, const VisitStatementInput& input) override {
        auto function_value = emit(expr->function).unqualified();

        if (auto pointer_type = type_cast<PointerType>(function_value.type)) {
            function_value = Value(ValueKind::LVALUE, pointer_type->base_type, get_rvalue(function_value));
        }

        if (auto function_type = type_cast<FunctionType>(function_value.type)) {
            auto result_type = function_type->return_type;
            if (outcome == EmitOutcome::TYPE) return VisitStatementOutput(result_type);

            // todo check number of parameters match

            vector<LLVMValueRef> llvm_params;
            llvm_params.reserve(function_type->parameter_types.size() + 1);

            if (auto member_expr = dynamic_cast<MemberExpr*>(expr->function)) {
                // todo convert types, e.g. in case it drops qualifiers
                llvm_params.push_back(emit_member_expr_object(member_expr).get_lvalue());
            }

            for (size_t i = 0; i < expr->parameters.size(); ++i) {
                auto param_expr = expr->parameters[i];
                auto expected_type = function_type->parameter_types[i];
                auto param_value = convert_to_type(param_expr, expected_type, ConvKind::IMPLICIT);
                llvm_params.push_back(get_rvalue(param_value));
            }

            auto result = LLVMBuildCall2(builder, function_type->llvm_type(), function_value.get_lvalue(), llvm_params.data(), llvm_params.size(), "");
            return VisitStatementOutput(result_type, result);
        }

        message(Severity::ERROR, expr->location) << "called object type '" << PrintType(function_value.type) << "' is not a function or function pointer\n";

        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(CastExpr* expr, const VisitStatementInput& input) override {
        if (outcome == EmitOutcome::TYPE) return VisitStatementOutput(expr->type);

        auto value = convert_to_type(expr->expr, expr->type, ConvKind::EXPLICIT);
        return VisitStatementOutput(value);
    }

    virtual VisitStatementOutput visit(ConditionExpr* expr, const VisitStatementInput& input) override {
        auto then_type = get_expr_type(expr->then_expr)->unqualified();
        auto else_type = get_expr_type(expr->else_expr)->unqualified();

        auto result_type = usual_arithmetic_conversions(then_type, else_type);
        if (!result_type) {
            // TODO there are other combinations of types that are valid for condition expressions
            message(Severity::ERROR, expr->location) << "incompatible conditional operand types '" << PrintType(then_type) << "' and '" << PrintType(else_type) << "'\n";
            pause_messages();
            return VisitStatementOutput(Value::of_zero_int());
        }

        if (outcome == EmitOutcome::TYPE) return VisitStatementOutput(result_type);

        LLVMBasicBlockRef alt_blocks[2] = {
            append_block("then"),
            append_block("else"),
        };
        auto merge_block = append_block("merge");

        auto condition_value = convert_to_type(expr->condition, IntegerType::of(IntegerSignedness::UNSIGNED, IntegerSize::BOOL), ConvKind::IMPLICIT);
        LLVMBuildCondBr(builder, get_rvalue(condition_value), alt_blocks[0], alt_blocks[1]);

        LLVMValueRef alt_values[2];

        LLVMPositionBuilderAtEnd(builder, alt_blocks[0]);
        alt_values[0] = get_rvalue(convert_to_type(expr->then_expr, result_type, ConvKind::IMPLICIT));
        LLVMBuildBr(builder, merge_block);

        LLVMPositionBuilderAtEnd(builder, alt_blocks[1]);
        alt_values[1] = get_rvalue(convert_to_type(expr->else_expr, result_type, ConvKind::IMPLICIT));
        LLVMBuildBr(builder, merge_block);

        LLVMPositionBuilderAtEnd(builder, merge_block);
        Value result;
        if (result_type->unqualified() != &VoidType::it) {
            auto phi_value = LLVMBuildPhi(builder, result_type->llvm_type(), "");
            LLVMAddIncoming(phi_value, alt_values, alt_blocks, 2);
            result = Value(result_type, phi_value);
        } else {
            result = Value(result_type);
        }

        return VisitStatementOutput(result);
    }

    virtual VisitStatementOutput visit(DereferenceExpr* expr, const VisitStatementInput& input) override {
        auto value = emit(expr->expr).unqualified();
        auto pointer_type = type_cast<PointerType>(value.type);
        auto result_type = pointer_type->base_type;
        if (outcome == EmitOutcome::TYPE) return VisitStatementOutput(result_type);

        return VisitStatementOutput(Value(ValueKind::LVALUE, result_type, get_rvalue(value)));
    }

    virtual VisitStatementOutput visit(EntityExpr* expr, const VisitStatementInput& input) override {
        auto declarator = expr->declarator->primary;
        auto result_type = declarator->type;

        if (outcome == EmitOutcome::TYPE) return VisitStatementOutput(result_type);

        if (auto enum_constant = declarator->enum_constant()) {
            if (!enum_constant->ready) {
                message(Severity::ERROR, expr->location) << "enum constant '" << *declarator->identifier << "' not yet available\n";
            }

            auto type = result_type;
            while (auto enum_type = type_cast<EnumType>(type->unqualified())) {
                type = enum_type->base_type;
            }

            auto int_type = type_cast<IntegerType>(type);
            return VisitStatementOutput(Value::of_int(int_type, enum_constant->value).bit_cast(result_type));

        } else if (auto entity = declarator->entity()) {
            // EntityPass ensures that all functions and globals are created before the Emitter pass.
            assert(entity->value.get_lvalue());
            return VisitStatementOutput(entity->value);
        } else {
            message(Severity::ERROR, expr->location) << "identifier is not an expression\n";
            pause_messages();
            throw EmitError(true);
        }

        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(IncDecExpr* expr, const VisitStatementInput& input) override {
        auto lvalue = emit(expr->expr).unqualified();
        auto before_value = lvalue.load(builder);

        const Type* result_type = before_value.type;
        if (outcome == EmitOutcome::TYPE) return VisitStatementOutput(result_type);

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
        return VisitStatementOutput(expr->postfix ? before_value : after_value);
    }

    Value emit_member_expr_object(MemberExpr* expr) {
        auto object = emit(expr->object).unqualified();
        if (auto pointer_type = type_cast<PointerType>(object.type)) {
            object = Value(ValueKind::LVALUE, object.type, get_rvalue(object));
        }
        return object;
    }

    virtual VisitStatementOutput visit(MemberExpr* expr, const VisitStatementInput& input) override {
        auto object_type = get_expr_type(expr->object)->unqualified();
        auto message_type = object_type;

        bool dereferenced{};
        if (auto pointer_type = type_cast<PointerType>(object_type)) {
            dereferenced = true;
            object_type = pointer_type->base_type;
        }

        if (auto struct_type = type_cast<StructuredType>(object_type)) {
            auto it = struct_type->scope.declarator_map.find(expr->identifier.text);
            if (it == struct_type->scope.declarator_map.end()) {
                message(Severity::ERROR, expr->location) << "no member named '" << expr->identifier << "' in '" << PrintType(struct_type) << "'\n";
                pause_messages();
                return VisitStatementOutput(Value::of_zero_int());
            }

            if (dereferenced) {
                if (expr->op == '.') {
                    message(Severity::ERROR, expr->location) << "type '" << PrintType(message_type) << "' is a pointer; consider using the '->' operator instead of '.'\n";
                }
            } else {
                if (expr->op == TOK_PTR_OP) {
                    message(Severity::ERROR, expr->location) << "type '" << PrintType(message_type) << "' is not a pointer; consider using the '.' operator instead of '->'\n";
                }
            }

            auto member = it->second;

            auto result_type = member->type;
            if (outcome == EmitOutcome::TYPE) return VisitStatementOutput(result_type);

            if (auto member_variable = member->variable()) {
                auto object = emit_member_expr_object(expr);
                
                auto llvm_struct_type = struct_type->llvm_type();

                Value value(ValueKind::LVALUE, result_type, LLVMBuildInBoundsGEP2(builder, llvm_struct_type, object.get_lvalue(),
                                                                                  member_variable->member->gep_indices.data(), member_variable->member->gep_indices.size(),
                                                                                  expr->identifier.c_str()));
                value.bit_field = member_variable->member->bit_field.get();
                return VisitStatementOutput(value);
            }
            
            if (auto member_entity = member->entity()) {
                assert(member_entity->value.is_valid());
                return VisitStatementOutput(member_entity->value);
            }
        }

        message(Severity::ERROR, expr->object->location) << "type '" << PrintType(object_type) << "' does not have members\n";
        pause_messages();
        return VisitStatementOutput(Value::of_zero_int());
    }

    virtual VisitStatementOutput visit(SizeOfExpr* expr, const VisitStatementInput& input) override {
        auto zero = TranslationUnitContext::it->zero_size;
        auto llvm_target_data = TranslationUnitContext::it->llvm_target_data;

        auto result_type = IntegerType::of_size(IntegerSignedness::UNSIGNED);

        if (expr->type->partition() == TypePartition::INCOMPLETE) {
            return VisitStatementOutput(result_type, zero);
        }

        if (outcome == EmitOutcome::TYPE) return VisitStatementOutput(result_type);

        return VisitStatementOutput(result_type, size_const_int(LLVMStoreSizeOfType(llvm_target_data, expr->type->llvm_type())));
    }

    virtual VisitStatementOutput visit(SubscriptExpr* expr, const VisitStatementInput& input) override {
        auto zero = TranslationUnitContext::it->zero_size;

        auto left_value = emit(expr->left).unqualified();
        auto index_value = emit(expr->right).unqualified();

        if (type_cast<IntegerType>(left_value.type)) {
            swap(left_value, index_value);
        }

        if (auto pointer_type = type_cast<PointerType>(left_value.type)) {
            auto result_type = pointer_type->base_type;
            if (outcome == EmitOutcome::TYPE) return VisitStatementOutput(result_type);

            LLVMValueRef index = get_rvalue(index_value);
            return VisitStatementOutput(Value(
                ValueKind::LVALUE,
                result_type,
                LLVMBuildGEP2(builder, result_type->llvm_type(), get_rvalue(left_value), &index, 1, "")));
        }

        if (auto array_type = type_cast<ArrayType>(left_value.type)) {
            auto result_type = array_type->element_type;
            if (outcome == EmitOutcome::TYPE) return VisitStatementOutput(result_type);

            LLVMValueRef indices[2] = { zero, get_rvalue(index_value) };
            return VisitStatementOutput(Value(
                ValueKind::LVALUE,
                result_type,
                LLVMBuildGEP2(builder, array_type->llvm_type(), left_value.get_lvalue(), indices, 2, "")));
        }

        assert(false);
        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(IntegerConstant* constant, const VisitStatementInput& input) override {
        return VisitStatementOutput(constant->value);
    }

    virtual VisitStatementOutput visit(FloatingPointConstant* constant, const VisitStatementInput& input) override {
        return VisitStatementOutput(constant->value);
    }

    virtual VisitStatementOutput visit(StringConstant* constant, const VisitStatementInput& input) override {
        auto result_type = ResolvedArrayType::of(ArrayKind::COMPLETE,
                                                 QualifiedType::of(constant->character_type, QUALIFIER_CONST),
                                                 constant->value.length + 1);
        if (outcome == EmitOutcome::TYPE) return VisitStatementOutput(result_type);

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

        return VisitStatementOutput(result_type, llvm_constant);
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

    try {
        return emitter.emit(const_cast<Expr*>(expr)).type;
    } catch (EmitError&) {
        return IntegerType::default_type();
    }
}

Value fold_expr(const Expr* expr) {
    static const EmitOptions options;
    Emitter emitter(EmitOutcome::FOLD, options);

    try {
        return emitter.emit(const_cast<Expr*>(expr));
    } catch (EmitError& e) {
        if (!e.error_reported) {
            message(Severity::ERROR, expr->location) << "not a constant expression\n";
        }

        auto type = get_expr_type(expr)->unqualified();
        return Value(type, LLVMConstNull(type->llvm_type()));
    }
}

LLVMModuleRef emit_pass(const ResolvedModule& resolved_module, const EmitOptions& options) {
    auto llvm_context = TranslationUnitContext::it->llvm_context;

    Module module;
    module.llvm_module = LLVMModuleCreateWithNameInContext("my_module", llvm_context);

    void entity_pass(const ResolvedModule& resolved_module, LLVMModuleRef llvm_module);
    entity_pass(resolved_module, module.llvm_module);

    Emitter emitter(EmitOutcome::IR, options);
    emitter.module = &module;

    emitter.emit(resolved_module.file_scope);
    for (auto scope: resolved_module.type_scopes) {
        emitter.emit(scope);
    }

    LLVMVerifyModule(module.llvm_module, LLVMPrintMessageAction, nullptr);

    return module.llvm_module;
}
