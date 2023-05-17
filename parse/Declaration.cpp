#include "Declaration.h"

#include "Constant.h"
#include "IdentifierMap.h"
#include "Message.h"
#include "pass/Visitor.h"
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

Declaration::Declaration(StorageClass storage_class, const Type* type, const Location& location)
    : LocationNode(location), storage_class(storage_class), type(type) {
}

Declaration::Declaration(const Location& location)
    : LocationNode(location) {
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

ostream& operator<<(ostream& stream, const vector<Declaration*>& items) {
    stream << '[';
    auto separate = false;
    for (auto item : items) {
        if (separate) stream << ", ";
        separate = true;
        stream << item;
    }
    return stream << ']';
}

Declarator::Declarator(const Declaration* declaration, const Type* type, InternedString identifier, DeclaratorDelegate* delegate, const Location& location)
    : primary(this), LocationNode(location), declaration(declaration), type(type), identifier(identifier), delegate(delegate) {
}

EnumConstant* Declarator::enum_constant() {
    return dynamic_cast<EnumConstant*>(delegate);
}

Entity* Declarator::entity() const {
    return dynamic_cast<Entity*>(delegate);
}

Variable* Declarator::variable() const {
    return dynamic_cast<Variable*>(delegate);
}

Function* Declarator::function() const {
    return dynamic_cast<Function*>(delegate);
}

TypeDelegate* Declarator::type_delegate() const {
    return dynamic_cast<TypeDelegate*>(delegate);
}

const Type* Declarator::to_type() const {
    if (!delegate) return nullptr;
    return delegate->to_type();
}

const char* Declarator::message_kind() const {
    return delegate->message_kind();
}

bool Declarator::is_member() const {
    if (auto delegate = variable()) {
        return delegate->storage_duration == StorageDuration::AGGREGATE;
    }
    return false;
}
    
void Declarator::message_see_declaration(const char* declaration_kind) const {
    auto& stream = message(Severity::INFO, location) << "see ";
    if (declaration_kind) {
        stream << declaration_kind;
    } else if (delegate->message_is_definition()) {
        stream << "definition";
    } else {
        stream << "declaration";
    }
    stream << " of " << message_kind() << " '"  << *identifier << "'\n";
}

VisitDeclaratorOutput Declarator::accept(Visitor& visitor, const VisitDeclaratorInput& input) {
    return delegate->accept(this, visitor, input);
}

void Declarator::print(ostream& stream) const {
    if (delegate) {
        delegate->print(this, stream);
    } else {
        stream << "\"placeholder\"";
    }
}

const Type* DeclaratorDelegate::to_type() const {
    return nullptr;
}


Entity::Entity(Linkage linkage): linkage(linkage) {

}

BitField::BitField(Expr* expr): expr(expr) {
}

void BitField::print(ostream& stream) const {
    stream << expr;
}

Variable::Variable(Linkage linkage, StorageDuration storage_duration, Expr* initializer)
    : Entity(linkage), storage_duration(storage_duration), initializer(initializer) {
    if (storage_duration == StorageDuration::AGGREGATE) {
        member.reset(new MemberVariable);
    }
}

DeclaratorKind Variable::kind() const {
    return DeclaratorKind::VARIABLE;
}

const char* Variable::message_kind() const {
    if (storage_duration == StorageDuration::AGGREGATE) {
        return "member";
    } else {
        return "variable";
    }
}

bool Variable::message_is_definition() const {
    return initializer || linkage == Linkage::NONE;
}

VisitDeclaratorOutput Variable::accept(Declarator* declarator, Visitor& visitor, const VisitDeclaratorInput& input) {
    return visitor.visit(declarator, this, input);
}

void Variable::print(const Declarator* declarator, ostream& stream) const {
    stream << "[\"var\", \"" << linkage << storage_duration;

    stream << "\", " << declarator->type << ", \"" << *declarator->identifier  << "\"";
    if (initializer) {
        stream << ", " << initializer;
    }
    stream << ']';
}

Function::Function(Linkage linkage, bool inline_definition, vector<Declarator*>&& parameters, Statement* body)
    : Entity(linkage), inline_definition(inline_definition), parameters(move(parameters)), body(body) {
}

Function::Function(Linkage linkage): Entity(linkage) {
}

DeclaratorKind Function::kind() const {
    return DeclaratorKind::FUNCTION;
}

const char* Function::message_kind() const {
    return "function";
}

bool Function::message_is_definition() const {
    return body;
}

VisitDeclaratorOutput Function::accept(Declarator* declarator, Visitor& visitor, const VisitDeclaratorInput& input) {
    return visitor.visit(declarator, this, input);
}

void Function::print(const Declarator* declarator, ostream& stream) const {
    stream << "[\"fun\", \"" << linkage;

    if (inline_definition) {
        stream << 'i';
    }

    stream << "\", " << declarator->type << ", \"" << *declarator->identifier << '"';
    if (body) {
        stream << ", [";
        for (auto i = 0; i < parameters.size(); ++i) {
            if (i != 0) stream << ", ";
            auto identifier = *parameters[i]->identifier;
            stream << '"' << identifier << '"';
        }
        stream << "], " << body;
    }
    stream << ']';
}

DeclaratorKind TypeDelegate::kind() const {
    return DeclaratorKind::TYPE_DEF;
}

const char* TypeDelegate::message_kind() const {
    if (dynamic_cast<const StructType*>(type_def_type.declarator->type)) {
        return "struct";
    } else if (dynamic_cast<const UnionType*>(type_def_type.declarator->type)) {
        return "union";
    } else if (dynamic_cast<const UnionType*>(type_def_type.declarator->type)) {
        return "enum";
    }
    return "typedef";
}

const Type* TypeDelegate::to_type() const {
    return &type_def_type;
}

bool TypeDelegate::message_is_definition() const {
    return true;
}

VisitDeclaratorOutput TypeDelegate::accept(Declarator* declarator, Visitor& visitor, const VisitDeclaratorInput& input) {
    return visitor.visit(declarator, this, input);
}

void TypeDelegate::print(const Declarator* declarator, ostream& stream) const {
    stream << "[\"typedef\", " << declarator->type << ", \"" << *declarator->identifier  << "\"]";
}

EnumConstant::EnumConstant(const EnumType* type, Expr* constant): type(type), expr(constant) {
}

DeclaratorKind EnumConstant::kind() const {
    return DeclaratorKind::ENUM_CONSTANT;
}

const char* EnumConstant::message_kind() const {
    return "enum constant";
}

bool EnumConstant::message_is_definition() const {
    return true;
}

VisitDeclaratorOutput EnumConstant::accept(Declarator* declarator, Visitor& visitor, const VisitDeclaratorInput& input) {
    return visitor.visit(declarator, this, input);
}

void EnumConstant::print(const Declarator* declarator, ostream& stream) const {
    stream << "[\"ec\", \"" << *declarator->identifier << '"';
    if (ready) {
        stream << ", " << value;
    } else {
        if (expr) {
            stream << ", " << expr;
        }
    }
    stream << ']';
}
