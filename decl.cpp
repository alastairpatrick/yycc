#include "std.h"

#include "Decl.h"

#include "CompileContext.h"

Decl::Decl(StorageClass storage_class, const Type* type, const string* identifier, const Location& location)
    : ASTNode(location), storage_class(storage_class), type(type), identifier(identifier) {
}

const Type* Decl::to_type() const {
    return nullptr;
}

void Decl::redeclare(const Decl* redeclared) const {
    message(redeclared->location) << "error redeclaration of '" << redeclared->identifier << "'\n";
    message(location) << "see original declaration\n";
}

Variable::Variable(StorageClass storage_class, const Type* type, const string* identifier, shared_ptr<Expr> initializer, const Location& location)
    : Decl(storage_class, type, move(identifier), location), initializer(initializer) {
}

void Variable::print(std::ostream& stream) const {
    stream << "[\"var\", [" << storage_class << "], \"" << type << "\", \"" << identifier  << "\"";
    if (initializer) {
        stream << ", " << initializer;
    }
    stream << ']';
}

Function::Function(StorageClass storage_class, const FunctionType* type, const string* identifier, shared_ptr<Statement> body, const Location& location)
    : Decl(storage_class, type, identifier, location), body(move(body)) {
    // For function definitions only. Prototypes use Variable instead.
    assert(this->body);
}

void Function::print(std::ostream& stream) const {
    stream << "[\"fun\", [" << storage_class << "], \"" << type << "\", \"" << identifier  << "\", " << body << ']';
}

TypeDef::TypeDef(const Type* type, const string* identifier, const Location& location)
    : Decl(StorageClass::TYPEDEF, type, identifier, location) {
}

const Type* TypeDef::to_type() const {
    return type;
}

void TypeDef::redeclare(const Decl* redeclared) const {
    if (auto redefined = dynamic_cast<const TypeDef*>(redeclared)) {
        if (redefined->type != type) {
            message(redefined->location) << "error redefinition of '" << identifier << "' with different type\n";
            message(location) << "see original definition\n";
        }
    } else {
        Decl::redeclare(redeclared);
    }
}

void TypeDef::print(std::ostream& stream) const {
    stream << "[\"typedef\", \"" << type << "\", \"" << identifier  << "\"]";
}
