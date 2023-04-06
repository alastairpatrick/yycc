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

Declaration::Declaration(IdentifierScope scope, StorageClass storage_class, const Location& location)
    : ASTNode(location), scope(scope), storage_class(storage_class) {
}

Declaration::Declaration(IdentifierScope scope, const Location& location)
    : ASTNode(location), scope(scope) {
}

Linkage Declaration::linkage() const {
    if (storage_class == StorageClass::STATIC && scope == IdentifierScope::FILE) {
        return Linkage::INTERNAL;
    } else if (storage_class == StorageClass::EXTERN || scope == IdentifierScope::FILE) {
        return Linkage::EXTERNAL;
    } else {
        return Linkage::NONE;
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

void Declarator::compose(Declarator* later) {
    if (typeid(*later) != typeid(*this)) {
        message(Severity::ERROR, later->location) << "redeclaration of '" << identifier << "' with different type\n";
        message(Severity::INFO, location) << "see prior declaration\n";
    }

    if (later->type != type) {
        auto composite_type = compose_types(type, later->type);
        if (composite_type) {
            type = composite_type;
        } else {
            message(Severity::ERROR, later->location) << "redeclaration of '" << identifier << "' with different type\n";
            message(Severity::INFO, location) << "see prior declaration\n";
        }
    }

    if (later->declaration->linkage() == Linkage::INTERNAL && declaration->linkage() != Linkage::INTERNAL) {
        message(Severity::ERROR, later->location) << "static declaration of '" << identifier << "' follows non-static declaration\n";
        message(Severity::INFO, location) << "see prior declaration\n";
    }
}

Variable::Variable(const Declaration* declaration, const Identifier& identifier, const Location& location)
    : Declarator(declaration, new CompatibleType(), identifier, location), storage_duration(StorageDuration::STATIC) {
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
}

void Variable::compose(Declarator* later) {
    Declarator::compose(later);

    auto later_var = dynamic_cast<Variable*>(later);
    if (!later_var) return;

    if (later_var->initializer) {
        if (initializer) {
            message(Severity::ERROR, later->location) << "redefinition of '" << identifier << "'\n";
            message(Severity::INFO, location) << "see prior definition\n";
        } else {
            initializer = later_var->initializer;
        }
    }

    assert(later_var->storage_duration == storage_duration);
}

void Variable::print(ostream& stream) const {
    stream << "[\"var\", \"" << declaration->linkage() << storage_duration;

    stream << "\", " << type << ", \"" << identifier  << "\"";
    if (initializer) {
        stream << ", " << initializer;
    }
    stream << ']';
}

Function::Function(const Declaration* declaration, const Identifier& identifier, const Location& location)
    : Declarator(declaration, new CompatibleType(), identifier, location) {
}

Function::Function(const Declaration* declaration, const FunctionType* type, uint32_t specifiers, const Identifier& identifier, vector<Variable*>&& params, Statement* body, const Location& location)
    : Declarator(declaration, type, identifier, location), params(move(params)), body(body) {
    auto scope = declaration->scope;
    auto storage_class = declaration->storage_class;

    if ((storage_class != StorageClass::STATIC && storage_class != StorageClass::EXTERN && storage_class != StorageClass::NONE) ||
        (storage_class == StorageClass::STATIC && scope != IdentifierScope::FILE)) {
        message(Severity::ERROR, location) << "invalid storage class\n";
    }

    // It's very valuable to determine which functions with external linkage are inline definitions, because they don't need to be
    // written to the AST file; another translation unit is guaranteed to have an external definition.
    inline_definition = (declaration->linkage() == Linkage::EXTERNAL) && (specifiers & (1 << TOK_INLINE)) && (storage_class !=  StorageClass::EXTERN);
}

void Function::compose(Declarator* later) {
    Declarator::compose(later);

    auto later_fn = dynamic_cast<Function*>(later);
    if (!later_fn) return;

    if (later_fn->body) {
        if (body) {
            message(Severity::ERROR, later_fn->location) << "redefinition of '" << identifier << "'\n";
            message(Severity::INFO, location) << "see prior definition\n";
        } else {
            body = later_fn->body;
            params = move(later_fn->params);
        }
    }
  
    inline_definition = later_fn->inline_definition && inline_definition;
}

void Function::print(ostream& stream) const {
    stream << "[\"fun\", \"" << declaration->linkage();

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

TypeDef::TypeDef(const Declaration* declaration, const Identifier& identifier, const Location& location)
    : Declarator(declaration, new CompatibleType(), identifier, location) {
    
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

EnumConstant::EnumConstant(const Declaration* declaration, const Identifier& identifier, const Location& location)
    : Declarator(declaration, new CompatibleType(), identifier, location) {
}

EnumConstant::EnumConstant(Declaration* declaration, const Identifier& identifier, Expr* constant, const Location& location)
    : Declarator(declaration, IntegerType::default_type(), identifier, location), constant(constant) {
}

void EnumConstant::print(ostream& stream) const {
    stream << "[\"ec\", \"" << identifier << '"';
    if (constant) {
        stream << ", " << constant;
    }
    stream << ']';
}
