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

struct ASTNode: Printable {
    ASTNode(const Location& location): location(location) {}

    Location location;
};

typedef vector<shared_ptr<ASTNode>> ASTNodeVector;

ostream& operator<<(ostream& stream, const ASTNodeVector& items);

struct Decl: ASTNode {
    Decl(StorageClass storage_class, const Type* type, std::string identifier, const Location& location);

    StorageClass storage_class;
    const Type* type;
    string identifier;

    virtual void print(std::ostream& stream) const = 0;
};

struct Statement: ASTNode {
    Statement(const Location& location): ASTNode(location) {}
};

struct Expr: Statement {
    explicit Expr(const Location& location): Statement(location) {}

    virtual const Type* get_type() const = 0;
    virtual LLVMValueRef codegen(CodeGenContext* context) const = 0;
};


#endif
