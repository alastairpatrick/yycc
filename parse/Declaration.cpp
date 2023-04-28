#include "Declaration.h"

#include "Constant.h"
#include "IdentifierMap.h"
#include "Message.h"
#include "Type.h"
#include "visit/Visitor.h"

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
    : primary(this), location(location), declaration(declaration), type(type), identifier(identifier) {
}

Declarator::Declarator(const Declaration* declaration, const Identifier &identifier, const Location& location)
    : primary(this), location(location), declaration(declaration), identifier(identifier) {
}

EnumConstant* Declarator::enum_constant() {
    return dynamic_cast<EnumConstant*>(delegate);
}

Entity* Declarator::entity() {
    return dynamic_cast<Entity*>(delegate);
}

Variable* Declarator::variable() {
    return dynamic_cast<Variable*>(delegate);
}

Function* Declarator::function() {
    return dynamic_cast<Function*>(delegate);
}

TypeDef* Declarator::type_def() {
    return dynamic_cast<TypeDef*>(delegate);
}

const Type* Declarator::to_type() const {
    if (!delegate) return nullptr;
    return delegate->to_type();
}

VisitDeclaratorOutput Declarator::accept(Visitor& visitor, const VisitDeclaratorInput& input) {
    return delegate->accept(visitor, input);
}

void Declarator::print(ostream& stream) const {
    delegate->print(stream);
}

DeclaratorDelegate::DeclaratorDelegate(Declarator* declarator): declarator(declarator) {
}

const Type* DeclaratorDelegate::to_type() const {
    return nullptr;
}

Linkage DeclaratorDelegate::get_linkage() const {
    return Linkage::NONE;
}

Entity::Entity(Declarator* declarator)
    : DeclaratorDelegate(declarator) {
    auto storage_class = declarator->declaration->storage_class;
    auto scope = declarator->declaration->scope;

    if (storage_class == StorageClass::STATIC && scope == IdentifierScope::FILE) {
        linkage = Linkage::INTERNAL;
    } else if (storage_class == StorageClass::EXTERN || scope == IdentifierScope::FILE) {
        linkage = Linkage::EXTERNAL;
    } else {
        linkage = Linkage::NONE;
    }
}

Linkage Entity::get_linkage() const {
    return linkage;
}

BitField::BitField(Expr* expr): expr(expr) {
}

void BitField::print(ostream& stream) const {
    stream << expr;
}

Variable::Variable(Declarator* declarator, Expr* initializer, Expr* bit_field_size)
    : Entity(declarator), initializer(initializer) {
    if (bit_field_size) {
        bit_field = new BitField(bit_field_size);
    }
}

Variable::Variable(Declarator* declarator): Entity(declarator) {
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

DeclaratorKind Variable::kind() const {
    return DeclaratorKind::VARIABLE;
}

const char* Variable::error_kind() const {
    if (declarator->declaration->scope == IdentifierScope::STRUCTURED) {
        return "member";
    } else {
        return "variable";
    }
}

bool Variable::is_definition() const {
    return initializer;
}

VisitDeclaratorOutput Variable::accept(Visitor& visitor, const VisitDeclaratorInput& input) {
    return visitor.visit(declarator, this, input);
}

void Variable::print(ostream& stream) const {
    stream << "[\"var\", \"" << linkage << storage_duration();

    stream << "\", " << declarator->type << ", \"" << declarator->identifier  << "\"";
    if (initializer) {
        stream << ", " << initializer;
    }
    stream << ']';
}

Function::Function(Declarator* declarator, uint32_t specifiers, vector<Declarator*>&& parameters, Statement* body)
    : Entity(declarator), parameters(move(parameters)), body(body) {
    auto scope = declarator->declaration->scope;
    auto storage_class = declarator->declaration->storage_class;

    if ((storage_class != StorageClass::STATIC && storage_class != StorageClass::EXTERN && storage_class != StorageClass::NONE) ||
        (storage_class == StorageClass::STATIC && scope != IdentifierScope::FILE)) {
        message(Severity::ERROR, declarator->location) << "invalid storage class\n";
    }

    inline_definition = (linkage == Linkage::EXTERNAL) && (specifiers & (1 << TOK_INLINE)) && (storage_class !=  StorageClass::EXTERN);
}

Function::Function(Declarator* declarator): Entity(declarator) {
}

DeclaratorKind Function::kind() const {
    return DeclaratorKind::FUNCTION;
}

const char* Function::error_kind() const {
    return "function";
}

bool Function::is_definition() const {
    return body;
}

VisitDeclaratorOutput Function::accept(Visitor& visitor, const VisitDeclaratorInput& input) {
    return visitor.visit(declarator, this, input);
}

void Function::print(ostream& stream) const {
    stream << "[\"fun\", \"" << linkage;

    if (inline_definition) {
        stream << 'i';
    }

    stream << "\", " << declarator->type << ", \"" << declarator->identifier << '"';
    if (body) {
        stream << ", [";
        for (auto i = 0; i < parameters.size(); ++i) {
            if (i != 0) stream << ", ";
            auto identifier = parameters[i]->identifier;
            stream << '"' << identifier << '"';
        }
        stream << "], " << body;
    }
    stream << ']';
}

TypeDef::TypeDef(Declarator* declarator)
    : DeclaratorDelegate(declarator), type_def_type(declarator) {
    
}

DeclaratorKind TypeDef::kind() const {
    return DeclaratorKind::TYPE_DEF;
}

const char* TypeDef::error_kind() const {
    if (dynamic_cast<const StructType*>(type_def_type.declarator->type)) {
        return "struct";
    } else if (dynamic_cast<const UnionType*>(type_def_type.declarator->type)) {
        return "union";
    } else if (dynamic_cast<const UnionType*>(type_def_type.declarator->type)) {
        return "enum";
    }
    return "typedef";
}

const Type* TypeDef::to_type() const {
    return &type_def_type;
}

bool TypeDef::is_definition() const {
    return true;
}

VisitDeclaratorOutput TypeDef::accept(Visitor& visitor, const VisitDeclaratorInput& input) {
    return visitor.visit(declarator, this, input);
}

void TypeDef::print(ostream& stream) const {
    stream << "[\"typedef\", " << declarator->type << ", \"" << declarator->identifier  << "\"]";
}

EnumConstant::EnumConstant(Declarator* declarator)
    : DeclaratorDelegate(declarator) {
}

EnumConstant::EnumConstant(Declarator* declarator, Declarator* enum_tag, Expr* constant)
    : DeclaratorDelegate(declarator), enum_tag(enum_tag), constant_expr(constant) {
}

DeclaratorKind EnumConstant::kind() const {
    return DeclaratorKind::ENUM_CONSTANT;
}

const char* EnumConstant::error_kind() const {
    return "enum constant";
}

bool EnumConstant::is_definition() const {
    return true;
}

VisitDeclaratorOutput EnumConstant::accept(Visitor& visitor, const VisitDeclaratorInput& input) {
    return visitor.visit(declarator, this, input);
}

void EnumConstant::print(ostream& stream) const {
    stream << "[\"ec\", \"" << declarator->identifier << '"';
    if (declarator->status >= DeclaratorStatus::RESOLVED) {
        stream << ", " << constant_int;
    } else {
        if (constant_expr) {
            stream << ", " << constant_expr;
        }
    }
    stream << ']';
}
