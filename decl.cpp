#include "std.h"

#include "Decl.h"

#include "CompileContext.h"

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

Decl::Decl(IdentifierScope scope, StorageClass storage_class, const Type* type, const string* identifier, const Location& location)
    : ASTNode(location), scope(scope), type(type), identifier(identifier) {
    if (storage_class == StorageClass::STATIC && scope == IdentifierScope::FILE) {
        linkage = Linkage::INTERNAL;
    } else if (storage_class == StorageClass::EXTERN || scope == IdentifierScope::FILE) {
        linkage = Linkage::EXTERNAL;
    } else {
        linkage = Linkage::NONE;
    }
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

Variable::Variable(IdentifierScope scope, StorageClass storage_class, const Type* type, const string* identifier, Expr* initializer, const Location& location)
    : Decl(scope, storage_class, type, identifier, location), initializer(initializer) {
    if (storage_class == StorageClass::EXTERN || storage_class == StorageClass::STATIC || scope == IdentifierScope::FILE) {
        storage_duration = StorageDuration::STATIC;
    } else {
        storage_duration = StorageDuration::AUTO;
    }

    is_definition = storage_class != StorageClass::EXTERN || initializer != nullptr;
}

void Variable::redeclare(Decl* redeclared) {
    Decl::redeclare(redeclared);

    auto redeclared_var = dynamic_cast<Variable*>(redeclared);
    if (!redeclared_var) return;

    if (!initializer) {
        initializer = redeclared_var->initializer;
    }

    if (!is_definition) {
        is_definition = redeclared_var->is_definition;
    }

    assert(storage_duration == redeclared_var->storage_duration);
}

void Variable::print(std::ostream& stream) const {
    stream << "[\"var\", \"" << linkage << storage_duration;

    if (!is_definition) {
        stream << 'X';
    }

    stream << "\", \"" << type << "\", \"" << identifier  << "\"";
    if (initializer) {
        stream << ", " << initializer;
    }
    stream << ']';
}

Function::Function(IdentifierScope scope, StorageClass storage_class, const FunctionType* type, uint32_t specifiers, const string* identifier, vector<Variable*>&& params, Statement* body, const Location& location)
    : Decl(scope, storage_class, type, identifier, location), params(move(params)), body(body) {
    if ((storage_class != StorageClass::STATIC && storage_class != StorageClass::EXTERN && storage_class != StorageClass::NONE) ||
        (storage_class == StorageClass::STATIC && scope != IdentifierScope::FILE)) {
        message(location) << "error invalid storage class\n";
    }

    inline_definition = (linkage == Linkage::EXTERNAL) && (specifiers & (1 << TOK_INLINE)) && (storage_class !=  StorageClass::EXTERN);
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

    if (!body) {
        body = redeclared_fn->body;
        params = move(redeclared_fn->params);
    }

    inline_definition = inline_definition && (redeclared_fn->inline_definition || redeclared_fn->scope != IdentifierScope::FILE);
}

void Function::print(std::ostream& stream) const {
    stream << "[\"fun\", \"" << linkage;

    if (inline_definition) {
        stream << 'i';
    }

    stream << "\", \"" << type << "\", \"" << identifier << '"';
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

TypeDef::TypeDef(IdentifierScope scope, const Type* type, const string* identifier, const Location& location)
    : Decl(scope, StorageClass::TYPEDEF, type, identifier, location) {
}

const Type* TypeDef::to_type() const {
    return type;
}

void TypeDef::print(std::ostream& stream) const {
    stream << "[\"typedef\", \"" << type << "\", \"" << identifier  << "\"]";
}
