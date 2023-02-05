#include "std.h"
#include "Decl.h"

Decl::Decl(StorageClass storage_class, const Type* type, const string* identifier, const Location& location)
    : ASTNode(location), storage_class(storage_class), type(type), identifier(identifier) {
}

bool Decl::is_type() const {
    return false;
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

bool TypeDef::is_type() const {
    return true;
}

void TypeDef::print(std::ostream& stream) const {
    stream << "[\"typedef\", \"" << type << "\", \"" << identifier  << "\"]";
}
