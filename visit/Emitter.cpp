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

struct Emitter: Visitor {
    EmitOutcome outcome = EmitOutcome::TYPE;

    LLVMModuleRef module{};

    LLVMValueRef function{};
    const FunctionType* function_type;
    LLVMBuilderRef builder{};
    bool need_terminating_return = true;

    ~Emitter() {
        if (builder) LLVMDisposeBuilder(builder);
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
        declarator->accept(*this, VisitDeclaratorInput());
    }

    Value emit(Statement* statement) {
        return statement->accept(*this, VisitStatementInput()).value;
    }

    Value convert_to_type(Value value, const Type* target_type) {
        VisitTypeInput input;
        input.value = value;
        input.target_type = target_type;
        auto result = value.type->accept(*this, input).value;
        assert(result.type == target_type);
        return result;
    }

    void emit_function_definition(Declarator* declarator, Entity* entity) {
        function_type = dynamic_cast<const FunctionType*>(declarator->primary->type);
        function = LLVMAddFunction(module, declarator->identifier.name->data(), declarator->primary->type->llvm_type());

        LLVMBasicBlockRef entry = LLVMAppendBasicBlock(function, "entry");
        LLVMPositionBuilderAtEnd(builder, entry);

        for (size_t i = 0; i < entity->params.size(); ++i) {
            auto param = entity->params[i];
            auto param_entity = param->entity();

            auto storage = LLVMBuildAlloca(builder, param->type->llvm_type(), param->identifier.name->data());
            param_entity->value = Value(ValueKind::LVALUE, param->type, storage);
            LLVMBuildStore(builder, LLVMGetParam(function, i), storage);
        }

        need_terminating_return = true;
        emit(entity->body);
        if (need_terminating_return) {
            if (function_type->return_type == &VoidType::it) {
                LLVMBuildRetVoid(builder);
            } else {
                LLVMBuildRet(builder, LLVMConstNull(function_type->return_type->llvm_type()));
            }
        }

        function = nullptr;
    }

    virtual VisitDeclaratorOutput visit(Declarator* declarator, Entity* entity, const VisitDeclaratorInput& input) override {
        if (!entity->is_function() || !entity->body) return VisitDeclaratorOutput();

        assert(!function);
        emit_function_definition(declarator, entity);

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

        if (auto float_target = dynamic_cast<const FloatingPointType*>(target_type)) {
            if (float_target->size > type->size) {
                return VisitTypeOutput(target_type, LLVMBuildFPExt(builder, value.llvm_rvalue(builder), input.target_type->llvm_type(), "cvt"));
            } else {
                return VisitTypeOutput(target_type, LLVMBuildFPTrunc(builder, value.llvm_rvalue(builder), input.target_type->llvm_type(), "cvt"));
            }
        }

        if (auto int_target = dynamic_cast<const IntegerType*>(input.target_type)) {
            if (int_target->is_signed()) {
                return VisitTypeOutput(target_type, LLVMBuildFPToSI(builder, value.llvm_rvalue(builder), input.target_type->llvm_type(), "cvt"));
            } else {
                return VisitTypeOutput(target_type, LLVMBuildFPToUI(builder, value.llvm_rvalue(builder), input.target_type->llvm_type(), "cvt"));
            }
        }

        assert(false);  // TODO
        return VisitTypeOutput(value);
    }

    virtual VisitTypeOutput visit(const IntegerType* type, const VisitTypeInput& input) override {
        auto target_type = input.target_type;
        auto value = input.value;

        if (target_type == type) return VisitTypeOutput(value);

        if (auto int_target = dynamic_cast<const IntegerType*>(input.target_type)) {
            if (int_target->size == type->size) return VisitTypeOutput(value.signedness_cast(target_type));

            if (int_target->size > type->size) {
                if (type->is_signed()) {
                    return VisitTypeOutput(target_type, LLVMBuildSExt(builder, value.llvm_rvalue(builder), input.target_type->llvm_type(), ""));
                } else {
                    return VisitTypeOutput(target_type, LLVMBuildZExt(builder, value.llvm_rvalue(builder), input.target_type->llvm_type(), ""));
                }
            }

            return VisitTypeOutput(target_type, LLVMBuildTrunc(builder, value.llvm_rvalue(builder), input.target_type->llvm_type(), ""));
        }

        if (auto float_target = dynamic_cast<const FloatingPointType*>(input.target_type)) {
            if (type->is_signed()) {
                return VisitTypeOutput(target_type, LLVMBuildSIToFP(builder, value.llvm_rvalue(builder), input.target_type->llvm_type(), ""));
            } else {
                return VisitTypeOutput(target_type, LLVMBuildUIToFP(builder, value.llvm_rvalue(builder), input.target_type->llvm_type(), ""));
            }
        }

        assert(false); // TODO
        return VisitTypeOutput(value);
    }

    virtual VisitStatementOutput visit_default(Expr* expr, const VisitStatementInput& input) override {
        assert(false);
        return VisitStatementOutput();
    }

    virtual VisitStatementOutput visit(CompoundStatement* statement, const VisitStatementInput& input) override {
        for (auto node: statement->nodes) {
            need_terminating_return = true;
            emit(node);
        }
        return VisitStatementOutput();
    };

    virtual VisitStatementOutput visit(ReturnStatement* statement, const VisitStatementInput& input) override {
        if (statement->expr) {
            auto value = emit(statement->expr);
            value = convert_to_type(value, function_type->return_type);
            LLVMBuildRet(builder, value.llvm_rvalue(builder));
        } else {
            LLVMBuildRetVoid(builder);
        }
        need_terminating_return = false;
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

        if (auto type_as_int = dynamic_cast<const IntegerType*>(type)) {
            // Integer types smaller than int are promoted when an operation is performed on them.
            if (type_as_int->size < int_type->size || (type_as_int->signedness == IntegerSignedness::SIGNED && type_as_int->size == int_type->size)) {
                // If all values of the original type can be represented as an int, the value of the smaller type is converted to an int; otherwise, it is converted to an unsigned int.
                return int_type;
            }
        }
    
        return type;
    }

    // This is applied to binary expressions and to the second and third operands of a conditional expression.
    const Type* convert_arithmetic(const Type* left, const Type* right) {
        left = promote_integer(left);
        right = promote_integer(right);

        // If both operands have the same type, no further conversion is needed.
        if (left == right) return left;

        auto left_as_float = dynamic_cast<const FloatingPointType*>(left);
        auto right_as_float = dynamic_cast<const FloatingPointType*>(right);
        if (left_as_float) {
            if (right_as_float) {
                return FloatingPointType::of(max(left_as_float->size, right_as_float->size));
            }
            return left_as_float;
        }
        if (right_as_float) return right_as_float;

        auto left_as_int = dynamic_cast<const IntegerType*>(left);
        auto right_as_int = dynamic_cast<const IntegerType*>(right);

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

    virtual VisitStatementOutput visit(BinaryExpr* expr, const VisitStatementInput& input) override {
        bool is_assign = is_assignment(expr->op);

        auto left_value = emit(expr->left);
        auto right_value = emit(expr->right);
        auto result_type = is_assign ? left_value.type : convert_arithmetic(left_value.type, right_value.type);

        if (outcome == EmitOutcome::TYPE) return VisitStatementOutput(result_type);

        left_value = convert_to_type(left_value, result_type);
        right_value = convert_to_type(right_value, result_type);
        LLVMValueRef intermediate{};

        if (auto result_as_int = dynamic_cast<const IntegerType*>(result_type)) {
            switch (expr->op) {
              default:
                assert(false); // TODO
                break;
              case '=':
                intermediate = right_value.llvm_rvalue(builder);
                break;
              case '+':
              case TOK_ADD_ASSIGN:
                intermediate = LLVMBuildAdd(builder, left_value.llvm_rvalue(builder), right_value.llvm_rvalue(builder), "");
                break;
              case '-':
              case TOK_SUB_ASSIGN:
                intermediate = LLVMBuildSub(builder, left_value.llvm_rvalue(builder), right_value.llvm_rvalue(builder), "");
                break;
              case '*':
              case TOK_MUL_ASSIGN:
                intermediate = LLVMBuildMul(builder, left_value.llvm_rvalue(builder), right_value.llvm_rvalue(builder), "");
                break;
              case '/':
              case TOK_DIV_ASSIGN:
                if (result_as_int->is_signed()) {
                    intermediate = LLVMBuildSDiv(builder, left_value.llvm_rvalue(builder), right_value.llvm_rvalue(builder), "");
                } else {
                    intermediate = LLVMBuildUDiv(builder, left_value.llvm_rvalue(builder), right_value.llvm_rvalue(builder), "");
                }
                break;
            }
        }

        if (auto result_as_float = dynamic_cast<const FloatingPointType*>(result_type)) {
            switch (expr->op) {
              default:
                assert(false); // TODO
                break;
              case '=':
                intermediate = right_value.llvm_rvalue(builder);
                break;
              case '+':
              case TOK_ADD_ASSIGN:
                intermediate = LLVMBuildFAdd(builder, left_value.llvm_rvalue(builder), right_value.llvm_rvalue(builder), "");
                break;
              case '-':
              case TOK_SUB_ASSIGN:
                intermediate = LLVMBuildFSub(builder, left_value.llvm_rvalue(builder), right_value.llvm_rvalue(builder), "");
                break;
              case '*':
              case TOK_MUL_ASSIGN:
                intermediate = LLVMBuildFMul(builder, left_value.llvm_rvalue(builder), right_value.llvm_rvalue(builder), "");
                break;
              case '/':
              case TOK_DIV_ASSIGN:
                intermediate = LLVMBuildFDiv(builder, left_value.llvm_rvalue(builder), right_value.llvm_rvalue(builder), "");
                break;
            }
        }

        if (is_assign) {
            LLVMBuildStore(builder, intermediate, left_value.llvm_lvalue());
        }

        return VisitStatementOutput(result_type, intermediate);
    }

    virtual VisitStatementOutput visit(ConditionExpr* expr, const VisitStatementInput& input) override {
        Value condition_value = emit(expr->condition);
        Value then_value = emit(expr->then_expr);
        Value else_value = emit(expr->else_expr);

        auto result_type = convert_arithmetic(then_value.type, else_value.type);
    
        if (outcome == EmitOutcome::TYPE) return VisitStatementOutput(result_type);

        LLVMBasicBlockRef alt_blocks[2] = {
            LLVMAppendBasicBlock(function, "then"),
            LLVMAppendBasicBlock(function, "else"),
        };
        auto merge_block = LLVMAppendBasicBlock(function, "merge");

        auto cond_value = convert_to_type(condition_value, IntegerType::of(IntegerSignedness::UNSIGNED, IntegerSize::INT));
        LLVMBuildCondBr(builder, cond_value.llvm_rvalue(builder), alt_blocks[0], alt_blocks[1]);

        LLVMValueRef 
            alt_values[2];
        LLVMPositionBuilderAtEnd(builder, alt_blocks[0]);
        alt_values[0] = convert_to_type(then_value, result_type).llvm_rvalue(builder);
        LLVMBuildBr(builder, merge_block);

        LLVMPositionBuilderAtEnd(builder, alt_blocks[1]);
        alt_values[1] = convert_to_type(else_value, result_type).llvm_rvalue(builder);
        LLVMBuildBr(builder, merge_block);

        LLVMPositionBuilderAtEnd(builder, merge_block);
        auto phi_value = LLVMBuildPhi(builder, result_type->llvm_type(), "cond");
        LLVMAddIncoming(phi_value, alt_values, alt_blocks, 2);

        return VisitStatementOutput(result_type, phi_value);
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

    virtual VisitStatementOutput visit(IntegerConstant* constant, const VisitStatementInput& input) override {
        return VisitStatementOutput(constant->type, constant->value);
    }

    virtual VisitStatementOutput visit(FloatingPointConstant* constant, const VisitStatementInput& input) override {
        return VisitStatementOutput(constant->type, constant->value);
    }
};

const Type* get_expr_type(const Expr* expr) {
    Emitter emitter;
    emitter.outcome = EmitOutcome::TYPE;
    try {
        return emitter.emit(const_cast<Expr*>(expr)).type;
    } catch (EmitError&) {
        return IntegerType::default_type();
    }
}

Value fold_expr(const Expr* expr, unsigned long long error_value) {
    Emitter emitter;
    emitter.outcome = EmitOutcome::FOLD;
    emitter.builder = LLVMCreateBuilder();

    try {
        return emitter.emit(const_cast<Expr*>(expr));
    } catch (EmitError&) {
        return Value(IntegerType::default_type(), LLVMConstInt(IntegerType::default_type()->llvm_type(), error_value, false));
    }
}

LLVMModuleRef emit_pass(const ASTNodeVector& nodes) {
    Emitter emitter;
    emitter.outcome = EmitOutcome::IR;
    emitter.module = LLVMModuleCreateWithName("my_module");
    emitter.builder = LLVMCreateBuilder();

    for (auto node: nodes) {
        emitter.emit(node);
    }

    LLVMVerifyModule(emitter.module, LLVMPrintMessageAction, nullptr);

    return emitter.module;
}
