#include "std.h"

#include "Decl.h"

#include "CompileContext.h"

ostream& operator<<(ostream& stream, StorageClass storage_class) {
    switch (storage_class) {
    case StorageClass::NONE:
        break;
    case StorageClass::TYPEDEF:
        stream << "\"typedef\"";
        break;
    case StorageClass::EXTERN:
        stream << "\"extern\"";
        break;
    case StorageClass::STATIC:
        stream << "\"static\"";
        break;
    case StorageClass::AUTO:
        stream << "\"auto\"";
        break;
    case StorageClass::REGISTER:
        stream << "\"register\"";
        break;
    }
    return stream;
}

Decl::Decl(StorageClass storage_class, const Type* type, const string* identifier, const Location& location)
    : ASTNode(location), storage_class(storage_class), type(type), identifier(identifier) {
}

const Type* Decl::to_type() const {
    return nullptr;
}

bool Decl::is_function_definition() const {
    return false;
}

void Decl::redeclare(Decl* redeclared) {
    if (type != redeclared->type) {
        message(redeclared->location) << "error redeclaration of '" << redeclared->identifier << "' with different type\n";
        message(location) << "see original declaration\n";
    }
}

Variable::Variable(StorageClass storage_class, const Type* type, const string* identifier, Expr* initializer, const Location& location)
    : Decl(storage_class, type, move(identifier), location), initializer(initializer) {
}

DeclKind Variable::kind() const {
    return DeclKind::VARIABLE;
}

void Variable::print(std::ostream& stream) const {
    stream << "[\"var\", [" << storage_class << "], \"" << type << "\", \"" << identifier  << "\"";
    if (initializer) {
        stream << ", " << initializer;
    }
    stream << ']';
}

Function::Function(StorageClass storage, const FunctionType* type, const string* identifier, Statement* body, const Location& location)
    : Decl(storage, type, identifier, location), body(move(body)) {
    if (storage_class != StorageClass::STATIC && storage_class != StorageClass::EXTERN && storage_class != StorageClass::NONE) {
        storage_class = StorageClass::NONE;
        message(location) << "error invalid storage class for a function\n";
    }
}

DeclKind Function::kind() const {
    return DeclKind::FUNCTION;
}

bool Function::is_function_definition() const {
    return body != nullptr;
}

void Function::redeclare(Decl* redeclared) {
    Decl::redeclare(redeclared);

    auto redeclared_fn = dynamic_cast<Function*>(redeclared);
    if (!redeclared_fn) return;

    if (body && redeclared_fn->body) {
        message(redeclared_fn->location) << "error redefinition of '" << identifier << "'\n";
        message(location) << "see original definition\n";
    }

    if (!body) body = redeclared_fn->body;
}

void Function::print(std::ostream& stream) const {
    stream << "[\"fun\", [" << storage_class << "], \"" << type << "\", \"" << identifier << '"';
    if (body) {
        stream << ", " << body;
    }
    stream << ']';
}

TypeDef::TypeDef(const Type* type, const string* identifier, const Location& location)
    : Decl(StorageClass::TYPEDEF, type, identifier, location) {
}

DeclKind TypeDef::kind() const {
    return DeclKind::TYPEDEF;
}

const Type* TypeDef::to_type() const {
    return type;
}

void TypeDef::print(std::ostream& stream) const {
    stream << "[\"typedef\", \"" << type << "\", \"" << identifier  << "\"]";
}
