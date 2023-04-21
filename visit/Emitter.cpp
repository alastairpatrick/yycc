#include "Emitter.h"
#include "LLVM.h"
#include "Message.h"
#include "parse/Declaration.h"
#include "TranslationUnitContext.h"

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

struct SwitchConstruct {
    unordered_map<Expr*, LLVMBasicBlockRef> case_labels;
    LLVMBasicBlockRef default_label;
};

struct Emitter: Visitor {
    EmitOutcome outcome;

    Emitter* parent{};
    LLVMModuleRef module{};

    Declarator* function_declarator{};
    const FunctionType* function_type{};
    LLVMBuilderRef builder{};
    LLVMBuilderRef temp_builder{};
    LLVMValueRef function{};
    LLVMBasicBlockRef entry_block{};
    LLVMBasicBlockRef current_unreachable_block{};
    unordered_map<InternedString, LLVMBasicBlockRef> goto_labels;
    SwitchConstruct* innermost_switch{};

    Emitter(EmitOutcome outcome): outcome(outcome) {
        if (outcome != EmitOutcome::TYPE) {
            builder = LLVMCreateBuilder();
        }
        if (outcome == EmitOutcome::IR) {
            temp_builder = LLVMCreateBuilder();
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

    bool delete_unreachable_block(LLVMBasicBlockRef block) {
        if (block != current_unreachable_block) return false;

        LLVMDeleteBasicBlock(current_unreachable_block);
        current_unreachable_block = nullptr;
        return true;
    }

    void begin_unreachable_block() {
        delete_unreachable_block(LLVMGetInsertBlock(builder));
        current_unreachable_block = LLVMAppendBasicBlock(function, "unreachable");
        LLVMPositionBuilderAtEnd(builder, current_unreachable_block);
    }

    LLVMBasicBlockRef lookup_label(const Identifier& identifier) {
        auto& block = goto_labels[identifier.name];
        if (!block) {
            block = LLVMAppendBasicBlock(function, identifier_name(identifier));
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
                delete_unreachable_block(current_block);
            }
        }

        return statement->accept(*this, VisitStatementInput()).value;
    }

    Value convert_to_type(Value value, const Type* target_type) {
        assert(value.type->qualifiers() == 0);
        
        target_type = target_type->unqualified();

        VisitTypeInput input;
        input.value = value;
        input.target_type = target_type;
        auto result = value.type->accept(*this, input).value;
        assert(result.type == target_type);
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
        function = LLVMAddFunction(module, identifier_name(declarator->identifier), declarator->primary->type->llvm_type());

        entity->value = Value(ValueKind::LVALUE, function_type, function);

        entry_block = LLVMAppendBasicBlock(function, "");
        LLVMPositionBuilderAtEnd(builder, entry_block);

        for (size_t i = 0; i < entity->params.size(); ++i) {
            auto param = entity->params[i];
            auto param_entity = param->entity();

            auto storage = LLVMBuildAlloca(builder, param->type->llvm_type(), identifier_name(param->identifier));
            param_entity->value = Value(ValueKind::LVALUE, param->type, storage);
            LLVMBuildStore(builder, LLVMGetParam(function, i), storage);
        }

        emit(entity->body);

        if (!delete_unreachable_block(LLVMGetInsertBlock(builder))) {
            if (function_type->return_type == &VoidType::it) {
                LLVMBuildRetVoid(builder);
            } else {
                LLVMBuildRet(builder, LLVMConstNull(function_type->return_type->llvm_type()));
            }
        }

        function = nullptr;
    }

    void emit_variable(Declarator* declarator, Entity* entity) {
        auto llvm_type = declarator->type->llvm_type();

        LLVMValueRef initial_value{};
        if (entity->initializer) {
            initial_value = convert_to_type(emit(entity->initializer), declarator->type).llvm_rvalue(builder);
        } else {
            initial_value = LLVMConstNull(llvm_type);
        }

        if (entity->storage_duration() == StorageDuration::AUTO) {
            auto first_insn = LLVMGetFirstInstruction(entry_block);
            if (first_insn) {
                LLVMPositionBuilderBefore(temp_builder, first_insn);
            } else {
                LLVMPositionBuilderAtEnd(temp_builder, entry_block);
            }
            auto storage = LLVMBuildAlloca(temp_builder, llvm_type, identifier_name(declarator->identifier));

            LLVMBuildStore(builder, initial_value, storage);
            entity->value = Value(ValueKind::LVALUE, declarator->type, storage);

        } else if (entity->storage_duration() == StorageDuration::STATIC) {
            string name(*declarator->identifier.name);
            for (auto emitter = this; emitter->function_declarator; emitter = emitter->parent) {
                name = string(*emitter->function_declarator->identifier.name) + '.' + name;
            }

            auto global = LLVMAddGlobal(module, llvm_type, name.c_str());
            LLVMSetInitializer(global, initial_value);
            if (entity->linkage() != Linkage::EXTERNAL) {
                LLVMSetLinkage(global, LLVMInternalLinkage);
            }
            LLVMSetGlobalConstant(global, declarator->type->qualifiers() & QUAL_CONST);

            entity->value = Value(ValueKind::LVALUE, declarator->type, global);
        }
    }

    virtual VisitDeclaratorOutput visit(Declarator* declarator, Entity* entity, const VisitDeclaratorInput& input) override {
        assert(declarator == declarator->primary);
          
        if (entity->is_function()) {
            if (entity->body) {
                Emitter function_emitter(EmitOutcome::IR);
                function_emitter.module = module;
                function_emitter.parent = this;
                function_emitter.emit_function_definition(declarator, entity);
            }
        } else {
            emit_variable(declarator, entity);
        }

        return VisitDeclaratorOutput();
    }

    virtual VisitTypeOutput visit_default(const Type* type, const VisitTypeInput& input) override {
        if (input.target_type == type) return VisitTypeOutput(input.value);

        assert(false);  // TODO
        return VisitTypeOutput(input.value);
    }

    virtual VisitTypeOutput visit(const FloatingPointType* type, const VisitTypeInput& input) override {
        auto target_type = input.target_type;
        auto value = input.value;

        if (target_type == type) return VisitTypeOutput(value);

        if (auto float_target = type_cast<FloatingPointType>(target_type)) {
            return VisitTypeOutput(target_type, LLVMBuildFPCast(builder, value.llvm_rvalue(builder), input.target_type->llvm_type(), ""));
        }

        if (auto int_target = type_cast<IntegerType>(target_type)) {
            if (int_target->is_signed()) {
                return VisitTypeOutput(target_type, LLVMBuildFPToSI(builder, value.llvm_rvalue(builder), input.target_type->llvm_type(), ""));
            } else {
                return VisitTypeOutput(target_type, LLVMBuildFPToUI(builder, value.llvm_rvalue(builder), input.target_type->llvm_type(), ""));
            }
        }

        assert(false);  // TODO
        return VisitTypeOutput(value);
    }

    virtual VisitTypeOutput visit(const FunctionType* type, const VisitTypeInput& input) override {
        auto target_type = input.target_type;
        auto value = input.value;

        if (target_type == type) return VisitTypeOutput(value);

        if (auto pointer_type = type_cast<PointerType>(target_type)) {
            return VisitTypeOutput(target_type, value.llvm_lvalue());
        }

        assert(false);  // TODO
        return VisitTypeOutput(value);
    }

    virtual VisitTypeOutput visit(const IntegerType* type, const VisitTypeInput& input) override {
        auto target_type = input.target_type;
        auto value = input.value;

        if (target_type == type) return VisitTypeOutput(value);

        if (auto int_target = type_cast<IntegerType>(target_type)) {
            if (int_target->size == type->size) return VisitTypeOutput(value.bit_cast(target_type));
            return VisitTypeOutput(target_type, LLVMBuildIntCast2(builder, value.llvm_rvalue(builder), input.target_type->llvm_type(), type->is_signed(), ""));
        }

        if (type_cast<FloatingPointType>(target_type)) {
            if (type->is_signed()) {
                return VisitTypeOutput(target_type, LLVMBuildSIToFP(builder, value.llvm_rvalue(builder), input.target_type->llvm_type(), ""));
            } else {
                return VisitTypeOutput(target_type, LLVMBuildUIToFP(builder, value.llvm_rvalue(builder), input.target_type->llvm_type(), ""));
            }
        }

        if (type_cast<PointerType>(target_type)) {
            return VisitTypeOutput(target_type, LLVMBuildIntToPtr(builder, value.llvm_rvalue(builder), target_type->llvm_type(), ""));
        }

        assert(false); // TODO
        return VisitTypeOutput(value);
    }
    
    VisitTypeOutput visit(const PointerType* type, const VisitTypeInput& input) {
        auto target_type = input.target_type;
        auto value = input.value;

        if (type_cast<PointerType>(input.target_type)) {
            return VisitTypeOutput(value.bit_cast(input.target_type));
        }

        if (type_cast<IntegerType>(input.target_type)) {
            return VisitTypeOutput(target_type, LLVMBuildPtrToInt(builder, value.llvm_rvalue(builder), target_type->llvm_type(), ""));
        }

        assert(false); // TODO
        return VisitTypeOutput(input.value);
    }

    VisitTypeOutput visit(const QualifiedType* type, const VisitTypeInput& input) {
        return VisitTypeOutput(convert_to_type(input.value.bit_cast(type->base_type), input.target_type));
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
        if (statement->declaration) {
            emit(statement->declaration);
        }

        // TODO initialize

        auto loop_block = LLVMAppendBasicBlock(function, "for_l");
        auto body_block = LLVMAppendBasicBlock(function, "for_b");
        auto end_block = LLVMAppendBasicBlock(function, "for_e");
        LLVMBuildBr(builder, loop_block);
        LLVMPositionBuilderAtEnd(builder, loop_block);

        if (statement->condition) {
            auto condition_value = emit(statement->condition).unqualified();
            condition_value = convert_to_type(condition_value, IntegerType::of_bool());
            LLVMBuildCondBr(builder, condition_value.llvm_rvalue(builder), body_block, end_block);
        } else {
            LLVMBuildBr(builder, body_block);
        }

        LLVMPositionBuilderAtEnd(builder, body_block);
        emit(statement->body);
        if (statement->iterate) {
            emit(statement->iterate);
        }
        LLVMBuildBr(builder, loop_block);

        LLVMPositionBuilderAtEnd(builder, end_block);

        return VisitStatementOutput();
    }

    VisitStatementOutput visit(GoToStatement* statement, const VisitStatementInput& input) {
        auto target_block = lookup_label(statement->identifier);
        LLVMBuildBr(builder, target_block);

        begin_unreachable_block();

        return VisitStatementOutput();
    }

    VisitStatementOutput visit(IfElseStatement* statement, const VisitStatementInput& input) {
        auto then_block = LLVMAppendBasicBlock(function, "if_c");
        LLVMBasicBlockRef else_block;
        if (statement->else_statement) else_block = LLVMAppendBasicBlock(function, "if_a");
        auto end_block = LLVMAppendBasicBlock(function, "if_e");
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

        begin_unreachable_block();

        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(SwitchStatement* statement, const VisitStatementInput& input) override {
        auto parent_construct = innermost_switch;
        SwitchConstruct construct;
        innermost_switch = &construct;

        auto expr_value = emit(statement->expr).unqualified();
        construct.default_label = LLVMAppendBasicBlock(function, "");
        auto switch_value = LLVMBuildSwitch(builder, expr_value.llvm_rvalue(builder), construct.default_label, statement->cases.size());

        for (auto case_expr: statement->cases) {
            auto case_label = LLVMAppendBasicBlock(function, "");
            innermost_switch->case_labels[case_expr] = case_label;

            auto case_value = fold_expr(case_expr);
            if(!case_value.is_const_integer()) {
                // TODO: error
            }
            LLVMAddCase(switch_value, case_value.llvm_const_rvalue(), case_label);
        }

        begin_unreachable_block();
        emit(statement->body);

        innermost_switch = parent_construct;
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
            auto size_of_base_type = LLVMStoreSizeOfType(g_llvm_target_data, pointer_type->base_type->llvm_type());
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
            LLVMAppendBasicBlock(function, "then"),
            LLVMAppendBasicBlock(function, "else"),
        };
        auto merge_block = LLVMAppendBasicBlock(function, "merge");

        auto condition_value = emit(expr->condition).unqualified();
        condition_value = convert_to_type(condition_value, IntegerType::of(IntegerSignedness::UNSIGNED, IntegerSize::BOOL));
        LLVMBuildCondBr(builder, condition_value.llvm_rvalue(builder), alt_blocks[0], alt_blocks[1]);

        LLVMValueRef 
            alt_values[2];
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
        auto result_type = IntegerType::uintptr_type();

        if (!expr->type->is_complete()) {
            message(Severity::ERROR, expr->location) << "sizeof applied to incomplete type\n";
            return VisitStatementOutput(result_type, LLVMConstInt(result_type->llvm_type(), 0, false));;
        }

        if (outcome == EmitOutcome::TYPE) return VisitStatementOutput(result_type);

        auto size_int = LLVMStoreSizeOfType(g_llvm_target_data, expr->type->llvm_type());
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
};

const Type* get_expr_type(const Expr* expr) {
    Emitter emitter(EmitOutcome::TYPE);
    try {
        return emitter.emit(const_cast<Expr*>(expr)).type;
    } catch (EmitError&) {
        return IntegerType::default_type();
    }
}

Value fold_expr(const Expr* expr, unsigned long long error_value) {
    Emitter emitter(EmitOutcome::FOLD);

    try {
        return emitter.emit(const_cast<Expr*>(expr));
    } catch (EmitError&) {
        return Value(IntegerType::default_type(), LLVMConstInt(IntegerType::default_type()->llvm_type(), error_value, false));
    }
}

LLVMModuleRef emit_pass(const ASTNodeVector& nodes) {
    Emitter emitter(EmitOutcome::IR);
    emitter.module = LLVMModuleCreateWithName("my_module");

    for (auto node: nodes) {
        emitter.emit(node);
    }

    LLVMVerifyModule(emitter.module, LLVMPrintMessageAction, nullptr);

    return emitter.module;
}
