#include "Declaration.h"

#include "Constant.h"
#include "Message.h"
#include "Type.h"

ostream& operator<<(ostream& stream, Linkage linkage) {
    switch (linkage) {
      case Linkage::NONE:
        break;
      case Linkage::INTERNAL:
        stream << 'I';
        break;
      case Linkage::EXTERNAL:
        stream << 'E';
        break;
    }
    return stream;
}

ostream& operator<<(ostream& stream, StorageDuration duration) {
    switch (duration) {
      case StorageDuration::AUTO:
        break;
      case StorageDuration::STATIC:
        stream << 'S';
        break;
    }
    return stream;
}

Declaration::Declaration(IdentifierScope scope, StorageClass storage_class, const Type* base_type, const Location& location)
    : Declaration(scope, location) {
    initialize(storage_class, base_type);
}

Declaration::Declaration(IdentifierScope scope, const Location& location)
    : ASTNode(location), scope(scope) {
}

void Declaration::initialize(StorageClass storage_class, const Type* base_type) {
    this->storage_class = storage_class;
    this->base_type = base_type;

    if (storage_class == StorageClass::STATIC && scope == IdentifierScope::FILE) {
        linkage = Linkage::INTERNAL;
    } else if (storage_class == StorageClass::EXTERN || scope == IdentifierScope::FILE) {
        linkage = Linkage::EXTERNAL;
    } else {
        linkage = Linkage::NONE;
    }
}

void Declaration::print(ostream& stream) const {
    if (declarators.size() != 1) stream << '[';

    auto separate = false;
    for (auto& declarator : declarators) {
        if (separate) stream << ", ";
        separate = true;
        stream << declarator;
    }

    if (declarators.size() != 1) stream << ']';
}

Declarator::Declarator(const Declaration* declaration, const Type* type, const Identifier &identifier, const Location& location)
    : ASTNode(location), declaration(declaration), type(type), identifier(identifier) {
}

const Type* Declarator::to_type() const {
    return nullptr;
}

void Declarator::combine() {
    if (type != earlier->type || typeid(*this) != typeid(*earlier)) {
        message(Severity::ERROR, location) << "redeclaration of '" << identifier << "' with different type\n";
        message(Severity::INFO, earlier->location) << "see prior declaration\n";
    }

    if (declaration->linkage == Linkage::INTERNAL && earlier->declaration->linkage != Linkage::INTERNAL) {
        message(Severity::ERROR, location) << "static declaration of '" << identifier << "' follows non-static declaration\n";
        message(Severity::INFO, earlier->location) << "see prior declaration\n";
    }
}

Variable::Variable(const Declaration* declaration, const Type* type, const Identifier& identifier, Expr* initializer, Expr* bit_field_size, const Location& location)
    : Declarator(declaration, type, identifier, location), initializer(initializer), bit_field_size(bit_field_size) {
    auto scope = declaration->scope;
    auto storage_class = declaration->storage_class;

    if (storage_class == StorageClass::EXTERN || storage_class == StorageClass::STATIC || scope == IdentifierScope::FILE) {
        storage_duration = StorageDuration::STATIC;
    } else {
        storage_duration = StorageDuration::AUTO;
    }

    definition = initializer ? this : nullptr;
}

void Variable::combine() {
    Declarator::combine();

    auto earlier_var = dynamic_cast<Variable*>(earlier);
    if (!earlier_var) return;

    if (initializer && earlier_var->definition) {
        message(Severity::ERROR, location) << "redefinition of '" << identifier << "'\n";
        message(Severity::INFO, earlier->definition->location) << "see prior definition\n";
    }

    if (!definition) {
        definition = earlier_var->definition;
    }

    assert(storage_duration == earlier_var->storage_duration);
}

void Variable::print(ostream& stream) const {
    stream << "[\"var\", \"" << declaration->linkage << storage_duration;

    stream << "\", " << type << ", \"" << identifier  << "\"";
    if (initializer) {
        stream << ", " << initializer;
    }
    stream << ']';
}

Function::Function(const Declaration* declaration, const FunctionType* type, uint32_t specifiers, const Identifier& identifier, vector<Variable*>&& params, Statement* body, const Location& location)
    : Declarator(declaration, type, identifier, location), params(move(params)), body(body) {
    auto scope = declaration->scope;
    auto storage_class = declaration->storage_class;

    if ((storage_class != StorageClass::STATIC && storage_class != StorageClass::EXTERN && storage_class != StorageClass::NONE) ||
        (storage_class == StorageClass::STATIC && scope != IdentifierScope::FILE)) {
        message(Severity::ERROR, location) << "invalid storage class\n";
    }

    definition = body ? this : nullptr;

    // It's very valuable to determine which functions with external linkage are inline definitions, because they don't need to be
    // written to the AST file; another translation unit is guaranteed to have an external definition.
    inline_definition = (declaration->linkage == Linkage::EXTERNAL) && (specifiers & (1 << TOK_INLINE)) && (storage_class !=  StorageClass::EXTERN);
}

void Function::combine() {
    Declarator::combine();

    auto earlier_fn = dynamic_cast<Function*>(earlier);
    if (!earlier_fn) return;

    if (!definition) {
        definition = earlier_fn->definition;
    }

    if (body && earlier_fn->definition) {
        message(Severity::ERROR, location) << "redefinition of '" << identifier << "'\n";
        message(Severity::INFO, earlier_fn->definition->location) << "see prior definition\n";
    }

    inline_definition = inline_definition && earlier_fn->inline_definition;

    if (!inline_definition && definition) {
        auto definition_fn = dynamic_cast<Function*>(definition);
        definition_fn->inline_definition = false;
    }
}

void Function::print(ostream& stream) const {
    stream << "[\"fun\", \"" << declaration->linkage;

    if (inline_definition) {
        stream << 'i';
    }

    stream << "\", " << type << ", \"" << identifier << '"';
    if (body) {
        stream << ", [";
        for (auto i = 0; i < params.size(); ++i) {
            if (i != 0) stream << ", ";
            auto identifier = params[i]->identifier;
            stream << '"' << identifier << '"';
        }
        stream << "], " << body;
    }
    stream << ']';
}

TypeDef::TypeDef(const Declaration* declaration, const Type* type, const Identifier& identifier, const Location& location)
    : Declarator(declaration, type, identifier, location) {
}

const Type* TypeDef::to_type() const {
    return type;
}

void TypeDef::print(ostream& stream) const {
    stream << "[\"typedef\", " << type << ", \"" << identifier  << "\"]";
}

EnumConstant::EnumConstant(Declaration* declaration, const Identifier& identifier, Expr* constant, const Location& location)
    : Declarator(declaration, IntegerType::default_type(), identifier, location), constant(constant) {
}

void EnumConstant::print(ostream& stream) const {
    stream << "[\"" << identifier << '"';
    if (constant) {
        stream << ", " << constant;
    }
    stream << ']';
}
