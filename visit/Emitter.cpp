#include "Emitter.h"
#include "lex/StringLiteral.h"
#include "LLVM.h"
#include "Message.h"
#include "parse/Declaration.h"
#include "TranslationUnitContext.h"

struct Emitter;

enum class EmitOutcome {
    TYPE,
    FOLD,
    IR,
};

const char* identifier_name(const Identifier& identifier) {
    auto name = identifier.name;
    if (name->empty()) return "";

    return name->data();
}

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

    void emit(Declarator* declarator) {
        declarator = declarator->primary;
        if (declarator->status >= DeclaratorStatus::EMITTED) return;
        assert(declarator->status == DeclaratorStatus::RESOLVED);

        declarator->accept(*this, VisitDeclaratorInput());

        declarator->status = DeclaratorStatus::EMITTED;
    }

    LLVMBasicBlockRef append_block(const char* name) {
        auto llvm_context = TranslationUnitContext::it->llvm_context;
        return LLVMAppendBasicBlockInContext(llvm_context, function, name);
    }

    LLVMBasicBlockRef lookup_label(const Identifier& identifier) {
        auto& block = goto_labels[identifier.name];
        if (!block) {
            block = append_block(identifier_name(identifier));
        }
        return block;
    }

    Value emit(Statement* statement) {
        if (statement->labels.size()) {
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

        return statement->accept(*this, VisitStatementInput()).value;
    }

    Value convert_to_type(Value value, const Type* dest_type) {
        assert(value.type->qualifiers() == 0);
        
        dest_type = dest_type->unqualified();

        if (value.type == dest_type) return value;

        VisitTypeInput input;
        input.value = value;
        input.dest_type = dest_type;
        auto result = value.type->accept(*this, input).value;
        assert(result.type == dest_type);
        return result;
    }

    template <typename T, typename U>
    const T* type_cast(const U* type) {
        assert(type->qualifiers() == 0);
        return dynamic_cast<const T*>(type);
    }

    void emit_function_definition(Declarator* declarator, Entity* entity) {
        function_declarator = declarator;
        function_type = type_cast<FunctionType>(declarator->primary->type);
        function = LLVMAddFunction(module->llvm_module, identifier_name(declarator->identifier), declarator->primary->type->llvm_type());

        entity->value = Value(ValueKind::LVALUE, function_type, function);

        entry_block = append_block("");
        unreachable_block = append_block("");
        LLVMPositionBuilderAtEnd(builder, entry_block);

        for (size_t i = 0; i < entity->parameters.size(); ++i) {
            auto param = entity->parameters[i];
            auto param_entity = param->entity();

            auto storage = LLVMBuildAlloca(builder, param->type->llvm_type(), identifier_name(param->identifier));
            param_entity->value = Value(ValueKind::LVALUE, param->type, storage);
            LLVMBuildStore(builder, LLVMGetParam(function, i), storage);
        }

        emit(entity->body);

        if (function_type->return_type == &VoidType::it) {
            LLVMBuildRetVoid(builder);
        } else {
            LLVMBuildRet(builder, LLVMConstNull(function_type->return_type->llvm_type()));
        }
        LLVMDeleteBasicBlock(unreachable_block);

        function = nullptr;
    }

    void emit_auto_initializer(Value dest, Expr* expr) {
        auto llvm_context = TranslationUnitContext::it->llvm_context;

        if (auto initializer = dynamic_cast<InitializerExpr*>(expr)) {
            if (auto array_type = type_cast<ResolvedArrayType>(dest.type)) {
                LLVMValueRef zero = LLVMConstInt(LLVMInt32TypeInContext(llvm_context), 0, false);
                for (size_t i = 0; i < array_type->size; ++i) {
                    LLVMValueRef indices[] = { zero, LLVMConstInt(LLVMInt64TypeInContext(llvm_context), i, false) };
                    LLVMValueRef dest_element = LLVMBuildGEP2(builder, array_type->llvm_type(), dest.llvm_lvalue(), indices, 2, "");
                    emit_auto_initializer(Value(ValueKind::LVALUE, array_type->element_type, dest_element), initializer->elements[i]);
                }
                return;
            }

            if (auto struct_type = type_cast<StructType>(dest.type)) {
                for (size_t i = 0; i < struct_type->members.size(); ++i) {
                    LLVMValueRef dest_element = LLVMBuildStructGEP2(builder, struct_type->llvm_type(), dest.llvm_lvalue(), i, "");
                    emit_auto_initializer(Value(ValueKind::LVALUE, struct_type->members[i]->type, dest_element), initializer->elements[i]);
                }

                return;
            }
        }

        auto value = convert_to_type(emit(expr), dest.type);
        if (dest.kind == ValueKind::LVALUE) {
            LLVMBuildStore(builder, value.llvm_rvalue(builder), dest.llvm_lvalue());
        }
    }
    
    Value emit_static_initializer(Value dest, Expr* expr) {
        if (auto initializer = dynamic_cast<InitializerExpr*>(expr)) {
            if (auto array_type = type_cast<ResolvedArrayType>(dest.type)) {
                vector<LLVMValueRef> values(array_type->size);
                for (size_t i = 0; i < array_type->size; ++i) {
                    values[i] = emit_static_initializer(array_type->element_type, initializer->elements[i]).llvm_const_rvalue();
                }

                return Value(array_type, LLVMConstArray(array_type->element_type->llvm_type(), values.data(), values.size()));
            }

            if (auto struct_type = type_cast<StructType>(dest.type)) {
                vector<LLVMValueRef> values(struct_type->members.size());
                for (size_t i = 0; i < struct_type->members.size(); ++i) {
                    values[i] = emit_static_initializer(struct_type->members[i]->type, initializer->elements[i]).llvm_const_rvalue();
                }

                return Value(struct_type, LLVMConstNamedStruct(struct_type->llvm_type(), values.data(), values.size()));
            }
        }

        return convert_to_type(emit(expr), dest.type);
    }

    void emit_variable(Declarator* declarator, Entity* entity) {
        auto type = declarator->primary->type;
        auto llvm_type = type->llvm_type();

        auto null_value = LLVMConstNull(llvm_type);

        if (entity->storage_duration() == StorageDuration::AUTO) {
            auto first_insn = LLVMGetFirstInstruction(entry_block);
            if (first_insn) {
                LLVMPositionBuilderBefore(temp_builder, first_insn);
            } else {
                LLVMPositionBuilderAtEnd(temp_builder, entry_block);
            }
            auto storage = LLVMBuildAlloca(temp_builder, llvm_type, identifier_name(declarator->identifier));
            if (options.initialize_variables) LLVMBuildStore(temp_builder, null_value, storage);

            entity->value = Value(ValueKind::LVALUE, type, storage);

            if (entity->initializer) {
                emit_auto_initializer(entity->value, entity->initializer);
            } else if (options.initialize_variables) {
                LLVMBuildStore(builder, null_value, storage);
            }


        } else if (entity->storage_duration() == StorageDuration::STATIC) {
            string name(*declarator->identifier.name);
            for (auto emitter = this; emitter->function_declarator; emitter = emitter->parent) {
                name = string(*emitter->function_declarator->identifier.name) + '.' + name;
            }

            auto global = LLVMAddGlobal(module->llvm_module, llvm_type, name.c_str());

            LLVMSetGlobalConstant(global, type->qualifiers() & QUAL_CONST);


            LLVMValueRef initial = null_value;
            if (entity->initializer) {
                initial = emit_static_initializer(Value(type), entity->initializer).llvm_const_rvalue();
            }
            LLVMSetInitializer(global, initial);

            if (entity->linkage() != Linkage::EXTERNAL) {
                LLVMSetLinkage(global, LLVMInternalLinkage);
            }

            entity->value = Value(ValueKind::LVALUE, type, global);
        }
    }

    virtual VisitDeclaratorOutput visit(Declarator* declarator, Entity* entity, const VisitDeclaratorInput& input) override {
        assert(declarator == declarator->primary);
          
        if (entity->is_function()) {
            if (entity->body) {
                Emitter function_emitter(EmitOutcome::IR, options);
                function_emitter.module = module;
                function_emitter.parent = this;
                function_emitter.emit_function_definition(declarator, entity);
            }
        } else {
            emit_variable(declarator, entity);
        }

        return VisitDeclaratorOutput();
    }

    virtual VisitTypeOutput visit_default(const Type* source_type, const VisitTypeInput& input) override {
        if (input.dest_type == source_type) return VisitTypeOutput(input.value);

        assert(false);  // TODO
        return VisitTypeOutput(input.value);
    }

    VisitTypeOutput visit(const ResolvedArrayType* source_type, const VisitTypeInput& input) {
        auto dest_type = input.dest_type;
        auto value = input.value;

        if (auto dest_array_type = type_cast<ResolvedArrayType>(dest_type)) {
            if (value.is_const() && source_type->element_type == dest_array_type->element_type && source_type->size <= dest_array_type->size) {
                LLVMValueRef source_array = value.llvm_const_rvalue();
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
                return VisitTypeOutput(dest_type, value.llvm_lvalue());
            }

            if (value.is_const()) {
                auto& global = module->reified_constants[value.llvm_const_rvalue()];
                if (!global) {
                    global = LLVMAddGlobal(module->llvm_module, value.type->llvm_type(), "const");
                    LLVMSetGlobalConstant(global, true);
                    LLVMSetLinkage(global, LLVMPrivateLinkage);
                    LLVMSetInitializer(global, value.llvm_const_rvalue());
                }
                return VisitTypeOutput(dest_type, global);
            }
        }

        assert(false);  // TODO
        return VisitTypeOutput(value); 
    }

    virtual VisitTypeOutput visit(const FloatingPointType* source_type, const VisitTypeInput& input) override {
        auto dest_type = input.dest_type;
        auto value = input.value;

        if (type_cast<FloatingPointType>(dest_type)) {
            return VisitTypeOutput(dest_type, LLVMBuildFPCast(builder, value.llvm_rvalue(builder), input.dest_type->llvm_type(), ""));
        }

        if (auto dest_int_type = type_cast<IntegerType>(dest_type)) {
            if (dest_int_type->is_signed()) {
                return VisitTypeOutput(dest_type, LLVMBuildFPToSI(builder, value.llvm_rvalue(builder), input.dest_type->llvm_type(), ""));
            } else {
                return VisitTypeOutput(dest_type, LLVMBuildFPToUI(builder, value.llvm_rvalue(builder), input.dest_type->llvm_type(), ""));
            }
        }

        assert(false);  // TODO
        return VisitTypeOutput(value);
    }

    virtual VisitTypeOutput visit(const FunctionType* source_type, const VisitTypeInput& input) override {
        auto dest_type = input.dest_type;
        auto value = input.value;

        if (auto pointer_type = type_cast<PointerType>(dest_type)) {
            return VisitTypeOutput(dest_type, value.llvm_lvalue());
        }

        assert(false);  // TODO
        return VisitTypeOutput(value);
    }

    virtual VisitTypeOutput visit(const IntegerType* source_type, const VisitTypeInput& input) override {
        auto dest_type = input.dest_type;
        auto value = input.value;

        if (auto int_target = type_cast<IntegerType>(dest_type)) {
            if (int_target->size == source_type->size) return VisitTypeOutput(value.bit_cast(dest_type));
            return VisitTypeOutput(dest_type, LLVMBuildIntCast2(builder, value.llvm_rvalue(builder), input.dest_type->llvm_type(), source_type->is_signed(), ""));
        }

        if (type_cast<FloatingPointType>(dest_type)) {
            if (source_type->is_signed()) {
                return VisitTypeOutput(dest_type, LLVMBuildSIToFP(builder, value.llvm_rvalue(builder), input.dest_type->llvm_type(), ""));
            } else {
                return VisitTypeOutput(dest_type, LLVMBuildUIToFP(builder, value.llvm_rvalue(builder), input.dest_type->llvm_type(), ""));
            }
        }

        if (type_cast<PointerType>(dest_type)) {
            return VisitTypeOutput(dest_type, LLVMBuildIntToPtr(builder, value.llvm_rvalue(builder), dest_type->llvm_type(), ""));
        }

        assert(false); // TODO
        return VisitTypeOutput(value);
    }
    
    VisitTypeOutput visit(const PointerType* source_type, const VisitTypeInput& input) {
        auto dest_type = input.dest_type;
        auto value = input.value;

        if (type_cast<PointerType>(input.dest_type)) {
            return VisitTypeOutput(value.bit_cast(input.dest_type));
        }

        if (type_cast<IntegerType>(input.dest_type)) {
            return VisitTypeOutput(dest_type, LLVMBuildPtrToInt(builder, value.llvm_rvalue(builder), dest_type->llvm_type(), ""));
        }

        assert(false); // TODO
        return VisitTypeOutput(input.value);
    }

    VisitTypeOutput visit(const QualifiedType* source_type, const VisitTypeInput& input) {
        return VisitTypeOutput(convert_to_type(input.value.bit_cast(source_type->base_type), input.dest_type));
    }

    virtual VisitStatementOutput visit_default(Expr* expr, const VisitStatementInput& input) override {
        assert(false);
        return VisitStatementOutput();
    }

    VisitStatementOutput visit_default(Statement* statement, const VisitStatementInput& input) {
        assert(false);
        return VisitStatementOutput();
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
            auto condition_value = emit(statement->condition).unqualified();
            condition_value = convert_to_type(condition_value, IntegerType::of_bool());
            LLVMBuildCondBr(builder, condition_value.llvm_rvalue(builder), body_block, end_block);
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

        auto condition_value = emit(statement->condition).unqualified();
        condition_value = convert_to_type(condition_value, IntegerType::of_bool());
        LLVMBuildCondBr(builder, condition_value.llvm_rvalue(builder), then_block, else_block);

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
        if (statement->expr) {
            auto value = emit(statement->expr).unqualified();
            value = convert_to_type(value, function_type->return_type->unqualified());
            LLVMBuildRet(builder, value.llvm_rvalue(builder));
        } else {
            LLVMBuildRetVoid(builder);
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
        auto switch_value = LLVMBuildSwitch(builder, expr_value.llvm_rvalue(builder), default_block, statement->cases.size());

        for (auto case_expr: statement->cases) {
            auto case_label = append_block("");
            innermost_switch->case_labels[case_expr] = case_label;

            auto case_value = fold_expr(case_expr);
            if(!case_value.is_const_integer()) {
                // TODO: error
            }
            LLVMAddCase(switch_value, case_value.llvm_const_rvalue(), case_label);
        }

        LLVMPositionBuilderAtEnd(builder, unreachable_block);

        emit(statement->body);

        LLVMBuildBr(builder, construct.break_block);
        LLVMPositionBuilderAtEnd(builder, construct.break_block);

        return VisitStatementOutput();
    }

    bool is_assignment(TokenKind token) {
        switch (token) {
          default:
            return false;
          case '=':
          case TOK_MUL_ASSIGN:
          case TOK_DIV_ASSIGN:
          case TOK_MOD_ASSIGN:
          case TOK_ADD_ASSIGN:
          case TOK_SUB_ASSIGN:
          case TOK_LEFT_ASSIGN:
          case TOK_RIGHT_ASSIGN:
          case TOK_AND_ASSIGN:
          case TOK_OR_ASSIGN:
          case TOK_XOR_ASSIGN:
            return true;
        }
    }

    const Type* promote_integer(const Type* type) {
        auto int_type = IntegerType::default_type();

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

        // For class pointer
        assert(false); // TODO
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

    Value emit_regular_binary_operation(BinaryExpr* expr, Value left_value, Value right_value) {
        bool is_assign = is_assignment(expr->op);
        auto convert_type = is_assign ? left_value.type : usual_arithmetic_conversions(left_value.type, right_value.type);
        auto result_type = convert_type;
        if (llvm_float_predicate(expr->op)) {
            result_type = IntegerType::of_bool();
        }

        if (outcome == EmitOutcome::TYPE) return Value(result_type);

        left_value = convert_to_type(left_value, convert_type);
        right_value = convert_to_type(right_value, convert_type);

        if (auto as_int = type_cast<IntegerType>(convert_type)) {
            switch (expr->op) {
              default:
                assert(false); // TODO
                break;
              case '=':
                return right_value;
              case '+':
              case TOK_ADD_ASSIGN:
                return Value(result_type, LLVMBuildAdd(builder, left_value.llvm_rvalue(builder), right_value.llvm_rvalue(builder), ""));
              case '-':
              case TOK_SUB_ASSIGN:
                return Value(result_type, LLVMBuildSub(builder, left_value.llvm_rvalue(builder), right_value.llvm_rvalue(builder), ""));
              case '*':
              case TOK_MUL_ASSIGN:
                return Value(result_type, LLVMBuildMul(builder, left_value.llvm_rvalue(builder), right_value.llvm_rvalue(builder), ""));
              case '/':
              case TOK_DIV_ASSIGN:
                if (as_int->is_signed()) {
                    return Value(result_type, LLVMBuildSDiv(builder, left_value.llvm_rvalue(builder), right_value.llvm_rvalue(builder), ""));
                } else {
                    return Value(result_type, LLVMBuildUDiv(builder, left_value.llvm_rvalue(builder), right_value.llvm_rvalue(builder), ""));
                }
              case TOK_LEFT_OP:
                return Value(result_type, LLVMBuildShl(builder, left_value.llvm_rvalue(builder), right_value.llvm_rvalue(builder), ""));
              case TOK_RIGHT_OP:
                if (as_int->is_signed()) {
                  return Value(result_type, LLVMBuildAShr(builder, left_value.llvm_rvalue(builder), right_value.llvm_rvalue(builder), ""));
                } else {
                  return Value(result_type, LLVMBuildLShr(builder, left_value.llvm_rvalue(builder), right_value.llvm_rvalue(builder), ""));
                }
              case '<':       
              case '>':       
              case TOK_LE_OP: 
              case TOK_GE_OP: 
              case TOK_EQ_OP: 
              case TOK_NE_OP:
                return Value(result_type, LLVMBuildICmp(builder,
                                                        llvm_int_predicate(as_int->is_signed(), expr->op),
                                                        left_value.llvm_rvalue(builder), right_value.llvm_rvalue(builder),
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
                return Value(result_type, LLVMBuildFAdd(builder, left_value.llvm_rvalue(builder), right_value.llvm_rvalue(builder), ""));
              case '-':
              case TOK_SUB_ASSIGN:
                return Value(result_type, LLVMBuildFSub(builder, left_value.llvm_rvalue(builder), right_value.llvm_rvalue(builder), ""));
              case '*':
              case TOK_MUL_ASSIGN:
                return Value(result_type, LLVMBuildFMul(builder, left_value.llvm_rvalue(builder), right_value.llvm_rvalue(builder), ""));
              case '/':
              case TOK_DIV_ASSIGN:
                return Value(result_type, LLVMBuildFDiv(builder, left_value.llvm_rvalue(builder), right_value.llvm_rvalue(builder), ""));
              case '<':       
              case '>':       
              case TOK_LE_OP: 
              case TOK_GE_OP: 
              case TOK_EQ_OP: 
              case TOK_NE_OP:
                return Value(result_type, LLVMBuildFCmp(builder,
                                                        llvm_float_predicate(expr->op),
                                                        left_value.llvm_rvalue(builder), right_value.llvm_rvalue(builder),
                                                        ""));
            }
        }

        assert(false);
        return Value();
    }

    Value emit_pointer_arithmetic_operation(BinaryExpr* expr, const PointerType* pointer_type, Value left_value, Value right_value) {
        auto llvm_target_data = TranslationUnitContext::it->llvm_target_data;

        if (expr->op == '+' || expr->op == TOK_ADD_ASSIGN) {
            if (outcome == EmitOutcome::TYPE) return Value(pointer_type);

            LLVMValueRef index = right_value.llvm_rvalue(builder);
            return Value(pointer_type, LLVMBuildGEP2(builder, pointer_type->base_type->llvm_type(), left_value.llvm_rvalue(builder), &index, 1, ""));

        } else if (expr->op == '-') {
            auto result_type = IntegerType::of_size(IntegerSignedness::SIGNED);
            if (outcome == EmitOutcome::TYPE) return Value(result_type);
            
            auto left_int = LLVMBuildPtrToInt(builder, left_value.llvm_rvalue(builder), result_type->llvm_type(), "");
            auto right_int = LLVMBuildPtrToInt(builder, right_value.llvm_rvalue(builder), result_type->llvm_type(), "");
            auto byte_diff = LLVMBuildSub(builder, left_int, right_int, "");
            auto size_of_base_type = LLVMStoreSizeOfType(llvm_target_data, pointer_type->base_type->llvm_type());
            auto result = LLVMBuildSDiv(builder, byte_diff, LLVMConstInt(result_type->llvm_type(), size_of_base_type, true), "");
            return Value(result_type, result);
        } else {
            assert(false); // TODO
            return Value();
        }
    }

    Value convert_array_to_pointer(Value value) {
        if (auto array_type = type_cast<ArrayType>(value.type)) {
            auto result_type = array_type->element_type->pointer_to();
            if (outcome == EmitOutcome::TYPE) return Value(result_type);

            auto zero = LLVMConstInt(IntegerType::of_size(IntegerSignedness::UNSIGNED)->llvm_type(), 0, false);
            LLVMValueRef indices[2] = {zero, zero};
            return Value(result_type,
                         LLVMBuildGEP2(builder, array_type->llvm_type(), value.llvm_lvalue(), indices, 2, ""));
        }

        return value;
    }

    virtual VisitStatementOutput visit(AddressExpr* expr, const VisitStatementInput& input) override {
        auto value = emit(expr->expr);
        auto result_type = value.type->pointer_to();
        if (outcome == EmitOutcome::TYPE) return VisitStatementOutput(result_type);

        return VisitStatementOutput(result_type, value.llvm_lvalue());
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
            intermediate = emit_regular_binary_operation(expr, left_value, right_value);
        }

        if (outcome == EmitOutcome::TYPE) return VisitStatementOutput(intermediate);

        if (is_assignment(expr->op)) {
            left_value.store(builder, intermediate);
        }

        return VisitStatementOutput(intermediate);
    }

    virtual VisitStatementOutput visit(CallExpr* expr, const VisitStatementInput& input) override {
        auto function_value = emit(expr->function).unqualified();

        if (auto pointer_type = type_cast<PointerType>(function_value.type)) {
            function_value = Value(ValueKind::LVALUE, pointer_type->base_type, function_value.llvm_rvalue(builder));
        }

        if (auto function_type = type_cast<FunctionType>(function_value.type)) {
            auto result_type = function_type->return_type;
            if (outcome == EmitOutcome::TYPE) return VisitStatementOutput(result_type);

            vector<LLVMValueRef> llvm_params(function_type->parameter_types.size());
            for (size_t i = 0; i < llvm_params.size(); ++i) {
                auto param_expr = expr->parameters[i];
                auto expected_type = function_type->parameter_types[i];
                auto param_value = convert_to_type(emit(param_expr).unqualified(), expected_type);
                llvm_params[i] = param_value.llvm_rvalue(builder);
            }

            auto result = LLVMBuildCall2(builder, function_type->llvm_type(), function_value.llvm_lvalue(), llvm_params.data(), llvm_params.size(), "");
            return VisitStatementOutput(result_type, result);
        }

        assert(false);
        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(ConditionExpr* expr, const VisitStatementInput& input) override {
        auto then_type = get_expr_type(expr->then_expr)->unqualified();
        auto else_type = get_expr_type(expr->else_expr)->unqualified();

        auto result_type = usual_arithmetic_conversions(then_type, else_type);    
        if (outcome == EmitOutcome::TYPE) return VisitStatementOutput(result_type);

        LLVMBasicBlockRef alt_blocks[2] = {
            append_block("then"),
            append_block("else"),
        };
        auto merge_block = append_block("merge");

        auto condition_value = emit(expr->condition).unqualified();
        condition_value = convert_to_type(condition_value, IntegerType::of(IntegerSignedness::UNSIGNED, IntegerSize::BOOL));
        LLVMBuildCondBr(builder, condition_value.llvm_rvalue(builder), alt_blocks[0], alt_blocks[1]);

        LLVMValueRef alt_values[2];

        LLVMPositionBuilderAtEnd(builder, alt_blocks[0]);
        Value then_value = emit(expr->then_expr).unqualified();
        alt_values[0] = convert_to_type(then_value, result_type).llvm_rvalue(builder);
        LLVMBuildBr(builder, merge_block);

        LLVMPositionBuilderAtEnd(builder, alt_blocks[1]);
        Value else_value = emit(expr->else_expr).unqualified();
        alt_values[1] = convert_to_type(else_value, result_type).llvm_rvalue(builder);
        LLVMBuildBr(builder, merge_block);

        LLVMPositionBuilderAtEnd(builder, merge_block);
        auto phi_value = LLVMBuildPhi(builder, result_type->llvm_type(), "cond");
        LLVMAddIncoming(phi_value, alt_values, alt_blocks, 2);

        return VisitStatementOutput(result_type, phi_value);
    }

    virtual VisitStatementOutput visit(DereferenceExpr* expr, const VisitStatementInput& input) override {
        auto value = emit(expr->expr).unqualified();
        auto pointer_type = type_cast<PointerType>(value.type);
        auto result_type = pointer_type->base_type;
        if (outcome == EmitOutcome::TYPE) return VisitStatementOutput(result_type);

        return VisitStatementOutput(Value(ValueKind::LVALUE, result_type, value.llvm_rvalue(builder)));
    }

    virtual VisitStatementOutput visit(EntityExpr* expr, const VisitStatementInput& input) override {
        const Type* result_type = expr->declarator->type;

        if (outcome == EmitOutcome::TYPE) return VisitStatementOutput(result_type);

        if (auto enum_constant = expr->declarator->enum_constant()) {
            return VisitStatementOutput(result_type, LLVMConstInt(result_type->llvm_type(), enum_constant->constant_int, true));
        } else if (auto entity = expr->declarator->entity()) {
            return VisitStatementOutput(entity->value);
        } else {
            message(Severity::ERROR, expr->location) << "identifier is not an expression\n";
            pause_messages();
            throw EmitError();
        }

        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(IncDecExpr* expr, const VisitStatementInput& input) override {
        auto store_value = emit(expr->expr).unqualified();
        auto before_value = store_value.load(builder);

        const Type* result_type = before_value.type;
        if (outcome == EmitOutcome::TYPE) return VisitStatementOutput(result_type);

        LLVMValueRef one = LLVMConstInt(result_type->llvm_type(), 1, false);

        Value after_value;
        switch (expr->op) {
          case TOK_INC_OP:
            after_value = Value(result_type, LLVMBuildAdd(builder, before_value.llvm_rvalue(builder), one, ""));
            break;
          case TOK_DEC_OP:
            after_value = Value(result_type, LLVMBuildSub(builder, before_value.llvm_rvalue(builder), one, ""));
            break;
        }

        store_value.store(builder, after_value);
        return VisitStatementOutput(expr->postfix ? before_value : after_value);
    }

    virtual VisitStatementOutput visit(SizeOfExpr* expr, const VisitStatementInput& input) override {
        auto llvm_target_data = TranslationUnitContext::it->llvm_target_data;

        auto result_type = IntegerType::uintptr_type();

        if (!expr->type->is_complete()) {
            message(Severity::ERROR, expr->location) << "sizeof applied to incomplete type\n";
            return VisitStatementOutput(result_type, LLVMConstInt(result_type->llvm_type(), 0, false));;
        }

        if (outcome == EmitOutcome::TYPE) return VisitStatementOutput(result_type);

        auto size_int = LLVMStoreSizeOfType(llvm_target_data, expr->type->llvm_type());
        return VisitStatementOutput(result_type, LLVMConstInt(result_type->llvm_type(), size_int, false));
    }

    virtual VisitStatementOutput visit(SubscriptExpr* expr, const VisitStatementInput& input) override {
        auto left_value = emit(expr->left).unqualified();
        auto index_value = emit(expr->right).unqualified();

        if (type_cast<IntegerType>(left_value.type)) {
            swap(left_value, index_value);
        }

        if (auto pointer_type = type_cast<PointerType>(left_value.type)) {
            auto result_type = pointer_type->base_type;
            if (outcome == EmitOutcome::TYPE) return VisitStatementOutput(result_type);

            LLVMValueRef index = index_value.llvm_rvalue(builder);
            return VisitStatementOutput(Value(
                ValueKind::LVALUE,
                result_type,
                LLVMBuildGEP2(builder, result_type->llvm_type(), left_value.llvm_rvalue(builder), &index, 1, "")));
        }

        if (auto array_type = type_cast<ArrayType>(left_value.type)) {
            auto result_type = array_type->element_type;
            if (outcome == EmitOutcome::TYPE) return VisitStatementOutput(result_type);

            auto zero = LLVMConstInt(IntegerType::of_size(IntegerSignedness::UNSIGNED)->llvm_type(), 0, false);
            LLVMValueRef indices[2] = { zero, index_value.llvm_rvalue(builder) };
            return VisitStatementOutput(Value(
                ValueKind::LVALUE,
                result_type,
                LLVMBuildGEP2(builder, array_type->llvm_type(), left_value.llvm_lvalue(), indices, 2, "")));
        }

        assert(false);
        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(IntegerConstant* constant, const VisitStatementInput& input) override {
        return VisitStatementOutput(constant->type, constant->value);
    }

    virtual VisitStatementOutput visit(FloatingPointConstant* constant, const VisitStatementInput& input) override {
        return VisitStatementOutput(constant->type, constant->value);
    }

    virtual VisitStatementOutput visit(StringConstant* constant, const VisitStatementInput& input) override {
        auto result_type = ResolvedArrayType::of(ArrayKind::COMPLETE,
                                                 QualifiedType::of(constant->character_type, QUAL_CONST),
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

Value fold_expr(const Expr* expr, unsigned long long error_value) {
    static const EmitOptions options;
    Emitter emitter(EmitOutcome::FOLD, options);

    try {
        return emitter.emit(const_cast<Expr*>(expr));
    } catch (EmitError&) {
        return Value(IntegerType::default_type(), LLVMConstInt(IntegerType::default_type()->llvm_type(), error_value, false));
    }
}

LLVMModuleRef emit_pass(const ASTNodeVector& nodes, const EmitOptions& options) {
    auto llvm_context = TranslationUnitContext::it->llvm_context;

    Module module;
    module.llvm_module = LLVMModuleCreateWithNameInContext("my_module", llvm_context);

    Emitter emitter(EmitOutcome::IR, options);
    emitter.module = &module;

    for (auto node: nodes) {
        emitter.emit(node);
    }

    LLVMVerifyModule(module.llvm_module, LLVMPrintMessageAction, nullptr);

    return module.llvm_module;
}
