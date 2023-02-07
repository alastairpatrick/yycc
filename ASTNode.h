#ifndef AST_H
#define AST_H

#include "llvm-c/Core.h"

#include "Location.h"
#include "Printable.h"
#include "Type.h"

enum class IdentifierScope;
enum class StorageClass;

struct ASTNode: Printable {
    explicit ASTNode(const Location& location);

    Location location;

    ASTNode* next_delete;
};

typedef vector<ASTNode*> ASTNodeVector;

ostream& operator<<(ostream& stream, const ASTNodeVector& items);

struct Decl: ASTNode {
    Decl(IdentifierScope scope, StorageClass storage_class, const Type* type, const string* identifier, const Location& location);

    IdentifierScope scope;
    StorageClass storage_class;
    const Type* type;
    const string* identifier;

    // Only one declaration per identifier is retained. Redundant indicates that this delaration is not that.
    // Redundant declarations are added to the AST.
    bool redundant = false;

    virtual const Type* to_type() const;
    virtual bool is_function_definition() const;
    virtual void redeclare(Decl* redeclared);
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
