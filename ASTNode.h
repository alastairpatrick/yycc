#ifndef AST_H
#define AST_H

#include "llvm-c/Core.h"

#include "Location.h"
#include "Printable.h"
#include "Type.h"

enum class DeclKind;
enum class StorageClass;

struct ASTNode: Printable {
    explicit ASTNode(const Location& location);

    Location location;

    ASTNode* next_delete;
};

typedef vector<ASTNode*> ASTNodeVector;

ostream& operator<<(ostream& stream, const ASTNodeVector& items);

struct Decl: ASTNode {
    Decl(StorageClass storage_class, const Type* type, const string* identifier, const Location& location);

    StorageClass storage_class;
    const Type* type;
    const string* identifier;

    virtual DeclKind kind() const = 0;
    virtual const Type* to_type() const;
    virtual bool is_function_definition() const;
    virtual void redeclare(const Decl* redeclared) const;
    virtual void print(std::ostream& stream) const = 0;
};

struct Statement: ASTNode {
    Statement(const Location& location): ASTNode(location) {}
};

struct Expr: Statement {
    explicit Expr(const Location& location): Statement(location) {}

    virtual const Type* get_type() const = 0;
    virtual LLVMValueRef generate_value(CodeGenContext* context) const = 0;
};


#endif
