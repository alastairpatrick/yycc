#ifndef AST_NODE_H
#define AST_NODE_H

#include "Identifier.h"
#include "Location.h"
#include "Printable.h"

struct CodeGenContext;
enum class IdentifierScope;
enum class StorageClass;
enum class Linkage;
struct Type;

struct ASTNode: Printable {
    explicit ASTNode(const Location& location);
    void operator=(const ASTNode&) = delete;

    Location location;
    ASTNode* next_delete;
};

typedef vector<ASTNode*> ASTNodeVector;

ostream& operator<<(ostream& stream, const ASTNodeVector& items);


struct Decl: ASTNode {
    Decl(IdentifierScope scope, StorageClass storage_class, const Type* type, const Identifier& identifier, const Location& location);

    IdentifierScope scope;
    Linkage linkage;
    const Type* type{};
    Identifier identifier;
    Decl* earlier{};
    Decl* definition{};

    virtual const Type* to_type() const;
    virtual bool is_function_definition() const;
    virtual void combine();
    virtual void print(ostream& stream) const = 0;
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
