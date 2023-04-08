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

Declaration::Declaration(IdentifierScope scope, StorageClass storage_class, const Type* type, const Location& location)
    : location(location), scope(scope), storage_class(storage_class), type(type) {
}

Declaration::Declaration(IdentifierScope scope, const Location& location)
    : location(location), scope(scope) {
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
    if (declarators.size() == 0) {
        stream << "[\"declare\", " << type << ']';
    } else {
        if (declarators.size() != 1) stream << '[';

        auto separate = false;
        for (auto& declarator : declarators) {
            if (separate) stream << ", ";
            separate = true;
            stream << declarator;
        }

        if (declarators.size() != 1) stream << ']';
    }
}

Declarator::Declarator(const Declaration* declaration, const Type* type, const Identifier &identifier, const Location& location)
    : location(location), declaration(declaration), type(type), identifier(identifier) {
}

Declarator::Declarator(const Declaration* declaration, const Identifier &identifier, const Location& location)
    : location(location), declaration(declaration), identifier(identifier) {
}

EnumConstant* Declarator::enum_constant() {
    return dynamic_cast<EnumConstant*>(kind);
}

Function* Declarator::function() {
    return dynamic_cast<Function*>(kind);
}

Variable* Declarator::variable() {
    return dynamic_cast<Variable*>(kind);
}

TypeDef* Declarator::type_def() {
    return dynamic_cast<TypeDef*>(kind);
}
    
const Type* Declarator::to_type() const {
    if (!kind) return nullptr;
    return kind->to_type();
}

void Declarator::compose(Declarator* later) {
    if (later->type != type) {
        auto composite_type = compose_types(type, later->type);
        if (composite_type) {
            type = composite_type;
        } else {
            message(Severity::ERROR, later->location) << "redeclaration of '" << identifier << "' with incompatible type\n";
            message(Severity::INFO, location) << "see prior declaration\n";
        }
    }

    if (later->kind && kind && typeid(*later->kind) != typeid(*kind)) {
        message(Severity::ERROR, later->location) << "redeclaration of '" << identifier << "' with different type\n";
        message(Severity::INFO, location) << "see prior declaration\n";
    }

    if (later->declaration->linkage() == Linkage::INTERNAL && declaration->linkage() != Linkage::INTERNAL) {
        message(Severity::ERROR, later->location) << "static declaration of '" << identifier << "' follows non-static declaration\n";
        message(Severity::INFO, location) << "see prior declaration\n";
    }

    if (kind) {
        kind->compose(later);
    } else {
        kind = later->kind;
    }
}

void Declarator::print(ostream& stream) const {
    kind->print(stream);
}

DeclaratorKind::DeclaratorKind(Declarator* declarator): declarator(declarator) {
}

const Type* DeclaratorKind::to_type() const {
    return nullptr;
}

Variable::Variable(Declarator* declarator)
    : DeclaratorKind(declarator) {
}

Variable::Variable(Declarator* declarator, Expr* initializer, Expr* bit_field_size)
    : DeclaratorKind(declarator), initializer(initializer), bit_field_size(bit_field_size) {
}

StorageDuration Variable::storage_duration() const {
    auto scope = declarator->declaration->scope;
    auto storage_class = declarator->declaration->storage_class;

    if (storage_class == StorageClass::EXTERN || storage_class == StorageClass::STATIC || scope == IdentifierScope::FILE) {
        return StorageDuration::STATIC;
    } else {
        return StorageDuration::AUTO;
    }
}

void Variable::compose(Declarator* later) {
    auto later_var = later->variable();
    if (!later_var) return;

    if (later_var->initializer) {
        if (initializer) {
            message(Severity::ERROR, later->location) << "redefinition of '" << declarator->identifier << "'\n";
            message(Severity::INFO, declarator->location) << "see prior definition\n";
        } else {
            initializer = later_var->initializer;
        }
    }

    assert(later_var->storage_duration() == storage_duration());
}

void Variable::print(ostream& stream) const {
    stream << "[\"var\", \"" << declarator->declaration->linkage() << storage_duration();

    stream << "\", " << declarator->type << ", \"" << declarator->identifier  << "\"";
    if (initializer) {
        stream << ", " << initializer;
    }
    stream << ']';
}

Function::Function(Declarator* declarator)
    : DeclaratorKind(declarator) {
}

Function::Function(Declarator* declarator, uint32_t specifiers, vector<Variable*>&& params, Statement* body)
    : DeclaratorKind(declarator), params(move(params)), body(body) {
    auto scope = declarator->declaration->scope;
    auto storage_class = declarator->declaration->storage_class;

    if ((storage_class != StorageClass::STATIC && storage_class != StorageClass::EXTERN && storage_class != StorageClass::NONE) ||
        (storage_class == StorageClass::STATIC && scope != IdentifierScope::FILE)) {
        message(Severity::ERROR, declarator->location) << "invalid storage class\n";
    }

    // It's very valuable to determine which functions with external linkage are inline definitions, because they don't need to be
    // written to the AST file; another translation unit is guaranteed to have an external definition.
    inline_definition = (declarator->declaration->linkage() == Linkage::EXTERNAL) && (specifiers & (1 << TOK_INLINE)) && (storage_class !=  StorageClass::EXTERN);
}

void Function::compose(Declarator* later) {
    auto later_fn = later->function();
    if (!later_fn) return;

    if (later_fn->body) {
        if (body) {
            message(Severity::ERROR, later->location) << "redefinition of '" << declarator->identifier << "'\n";
            message(Severity::INFO, declarator->location) << "see prior definition\n";
        } else {
            body = later_fn->body;
            params = move(later_fn->params);
        }
    }
  
    inline_definition = later_fn->inline_definition && inline_definition;
}

void Function::print(ostream& stream) const {
    stream << "[\"fun\", \"" << declarator->declaration->linkage();

    if (inline_definition) {
        stream << 'i';
    }

    stream << "\", " << declarator->type << ", \"" << declarator->identifier << '"';
    if (body) {
        stream << ", [";
        for (auto i = 0; i < params.size(); ++i) {
            if (i != 0) stream << ", ";
            auto identifier = params[i]->declarator->identifier;
            stream << '"' << identifier << '"';
        }
        stream << "], " << body;
    }
    stream << ']';
}

TypeDef::TypeDef(Declarator* declarator)
    : DeclaratorKind(declarator) {
    
}

const Type* TypeDef::to_type() const {
    return declarator->type;
}

void TypeDef::compose(Declarator* later) {
    // TODO
}

void TypeDef::print(ostream& stream) const {
    stream << "[\"typedef\", " << declarator->type << ", \"" << declarator->identifier  << "\"]";
}

EnumConstant::EnumConstant(Declarator* declarator)
    : DeclaratorKind(declarator) {
}

EnumConstant::EnumConstant(Declarator* declarator, Expr* constant)
    : DeclaratorKind(declarator), constant(constant) {
}

void EnumConstant::compose(Declarator* later) {
    // TODO
}

void EnumConstant::print(ostream& stream) const {
    stream << "[\"ec\", \"" << declarator->identifier << '"';
    if (constant) {
        stream << ", " << constant;
    }
    stream << ']';
}
