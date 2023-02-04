#ifndef AST_H
#define AST_H

#include "llvm-c/Core.h"

#include "Location.h"
#include "Printable.h"
#include "Type.h"


enum class StorageClass {
    NONE,
    TYPEDEF,
    EXTERN,
    STATIC,
    AUTO,
    REGISTER,
};

ostream& operator<<(ostream& stream, StorageClass storage_class);

struct DeclStatement: Printable {
    DeclStatement(const Location& location): location(location) {}

    Location location;
};

typedef vector<shared_ptr<DeclStatement>> DeclStatementList;

ostream& operator<<(ostream& stream, const DeclStatementList& items);

struct Decl: DeclStatement {
    Decl(StorageClass storage_class, const Type* type, std::string identifier, const Location& location);

    StorageClass storage_class;
    const Type* type;
    string identifier;

    virtual void print(std::ostream& stream) const = 0;
};

struct Statement: DeclStatement {
    Statement(const Location& location): DeclStatement(location) {}
};

struct Expr: Statement {
    explicit Expr(const Location& location): Statement(location) {}

    virtual const Type* get_type() const = 0;
    virtual LLVMValueRef codegen(CodeGenContext* context) const = 0;
};


#endif
