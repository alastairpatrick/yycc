#ifndef VISIT_VALUE_WRANGLER_H
#define VISIT_VALUE_WRANGLER_H

#include "TypeVisitor.h"
#include "Utility.h"
#include "Value.h"

struct Module;

struct ExprValue: Value {
    const LocationNode* node{};

    ExprValue() = default;
    ExprValue(const ExprValue&) = default;
    ExprValue(Value value, const LocationNode* node): Value(value), node(node) {}
    ExprValue(const Type* type, const LocationNode* node): Value(type), node(node) {}
    ExprValue(const Type* type, LLVMValueRef llvm, const LocationNode* node): Value(type, llvm), node(node) {}
    ExprValue(ValueKind kind, const Type* type, LLVMValueRef llvm, const LocationNode* node): Value(kind, type, llvm), node(node) {}

    ExprValue bit_cast(const Type* type) const {
        return ExprValue(Value::bit_cast(type), node);
    }
};

struct ValueResolver {
    virtual LLVMValueRef get_value(ExprValue value, bool for_move_expr) = 0;
};

struct TypeConverter: TypeVisitor {
    Module* module{};
    LLVMBuilderRef builder{};
    EmitOutcome outcome{};
    ValueResolver& resolver;

    TypeConverter(Module* module, LLVMBuilderRef builder, EmitOutcome outcome, ValueResolver& resolver);

    ExprValue convert_to_type(ExprValue value, const Type* dest_type, ConvKind kind);

private:
    ExprValue value;
    Value result;
    ConvKind conv_kind;

    virtual const Type* visit(const ResolvedArrayType* dest_type) override;
    virtual const Type* visit(const PointerType* dest_type) override;
    virtual const Type* visit(const IntegerType* dest_type) override;
    virtual const Type* visit(const FloatingPointType* dest_type) override;
    virtual const Type* visit(const EnumType* dest_type) override;
    virtual const Type* visit(const VoidType* dest_type) override;

    void convert_array_to_pointer();
    void convert_enum_to_int();
    LLVMValueRef get_value_internal();
};


#endif
