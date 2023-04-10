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
    return dynamic_cast<EnumConstant*>(delegate);
}

Entity* Declarator::entity() {
    return dynamic_cast<Entity*>(delegate);
}

TypeDef* Declarator::type_def() {
    return dynamic_cast<TypeDef*>(delegate);
}

const Type* Declarator::to_type() const {
    if (!delegate) return nullptr;
    return delegate->to_type();
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

    if (later->delegate && delegate && typeid(*later->delegate) != typeid(*delegate)) {
        message(Severity::ERROR, later->location) << "redeclaration of '" << identifier << "' with different type\n";
        message(Severity::INFO, location) << "see prior declaration\n";
    }

    if (later->delegate->linkage() == Linkage::INTERNAL && delegate->linkage() != Linkage::INTERNAL) {
        message(Severity::ERROR, later->location) << "static declaration of '" << identifier << "' follows non-static declaration\n";
        message(Severity::INFO, location) << "see prior declaration\n";
    }

    if (delegate) {
        delegate->compose(later);
    } else {
        delegate = later->delegate;
    }
}

void Declarator::print(ostream& stream) const {
    delegate->print(stream);
}

DeclaratorDelegate::DeclaratorDelegate(Declarator* declarator): declarator(declarator) {
}

Linkage DeclaratorDelegate::linkage() const {
    return Linkage::NONE;
}

const Type* DeclaratorDelegate::to_type() const {
    return nullptr;
}

Entity::Entity(Declarator* declarator)
    : DeclaratorDelegate(declarator) {
}

Entity::Entity(Declarator* declarator, Expr* initializer, Expr* bit_field_size)
    : DeclaratorDelegate(declarator), initializer(initializer), bit_field_size(bit_field_size) {
}

Entity::Entity(Declarator* declarator, uint32_t specifiers, vector<Entity*>&& params, Statement* body)
    : DeclaratorDelegate(declarator), params(move(params)), body(body) {
    auto scope = declarator->declaration->scope;
    auto storage_class = declarator->declaration->storage_class;

    if ((storage_class != StorageClass::STATIC && storage_class != StorageClass::EXTERN && storage_class != StorageClass::NONE) ||
        (storage_class == StorageClass::STATIC && scope != IdentifierScope::FILE)) {
        message(Severity::ERROR, declarator->location) << "invalid storage class\n";
    }

    // It's very valuable to determine which functions with external linkage are inline definitions, because they don't need to be
    // written to the AST file; another translation unit is guaranteed to have an external definition.
    inline_definition = (linkage() == Linkage::EXTERNAL) && (specifiers & (1 << TOK_INLINE)) && (storage_class !=  StorageClass::EXTERN);
}

StorageDuration Entity::storage_duration() const {
    auto scope = declarator->declaration->scope;
    auto storage_class = declarator->declaration->storage_class;

    if (storage_class == StorageClass::EXTERN || storage_class == StorageClass::STATIC || scope == IdentifierScope::FILE) {
        return StorageDuration::STATIC;
    } else {
        return StorageDuration::AUTO;
    }
}

bool Entity::is_function() const {
    auto type = declarator->type->unqualified();
    return dynamic_cast<const FunctionType*>(type);
}

DeclaratorKind Entity::kind() const {
    return DeclaratorKind::ENTITY;
}

Linkage Entity::linkage() const {
    auto storage_class = declarator->declaration->storage_class;
    auto scope = declarator->declaration->scope;

    if (storage_class == StorageClass::STATIC && scope == IdentifierScope::FILE) {
        return Linkage::INTERNAL;
    } else if (storage_class == StorageClass::EXTERN || scope == IdentifierScope::FILE) {
        return Linkage::EXTERNAL;
    } else {
        return Linkage::NONE;
    }
}

void Entity::compose(Declarator* later) {
    auto later_entity = later->entity();
    if (!later_entity) return;

    if (later_entity->initializer) {
        if (initializer) {
            message(Severity::ERROR, later->location) << "redefinition of '" << declarator->identifier << "'\n";
            message(Severity::INFO, declarator->location) << "see prior definition\n";
        } else {
            initializer = later_entity->initializer;
        }
    }
    
    if (later_entity->body) {
        if (body) {
            message(Severity::ERROR, later->location) << "redefinition of '" << declarator->identifier << "'\n";
            message(Severity::INFO, declarator->location) << "see prior definition\n";
        } else {
            body = later_entity->body;
            params = move(later_entity->params);
        }
    }
  
    inline_definition = later_entity->inline_definition && inline_definition;

    assert(later_entity->storage_duration() == storage_duration());
}

void Entity::print(ostream& stream) const {
    if (is_function()) {
        stream << "[\"fun\", \"" << linkage();

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
    } else {
        stream << "[\"var\", \"" << linkage() << storage_duration();

        stream << "\", " << declarator->type << ", \"" << declarator->identifier  << "\"";
        if (initializer) {
            stream << ", " << initializer;
        }
        stream << ']';
    }
}

TypeDef::TypeDef(Declarator* declarator)
    : DeclaratorDelegate(declarator), type_def_type(declarator) {
    
}

DeclaratorKind TypeDef::kind() const {
    return DeclaratorKind::TYPE_DEF;
}

const Type* TypeDef::to_type() const {
    return &type_def_type;
}

void TypeDef::compose(Declarator* later) {
    // TODO
}

void TypeDef::print(ostream& stream) const {
    stream << "[\"typedef\", " << declarator->type << ", \"" << declarator->identifier  << "\"]";
}

EnumConstant::EnumConstant(Declarator* declarator)
    : DeclaratorDelegate(declarator) {
}

EnumConstant::EnumConstant(Declarator* declarator, Expr* constant)
    : DeclaratorDelegate(declarator), constant(constant) {
}

DeclaratorKind EnumConstant::kind() const {
    return DeclaratorKind::ENUM_CONSTANT;
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
