#ifndef AST_NODE_H
#define AST_NODE_H

#include "Identifier.h"
#include "Location.h"
#include "Printable.h"

struct CodeGenContext;
struct Declaration;
enum class IdentifierScope;
enum class StorageClass;
enum class Linkage;
struct Type;

struct ASTNode: Printable {
    explicit ASTNode(const Location& location);
    void operator=(const ASTNode&) = delete;

    Location location;
    string_view text;
    ASTNode* next_delete;
};

typedef vector<ASTNode*> ASTNodeVector;

ostream& operator<<(ostream& stream, const ASTNodeVector& items);

struct Declarator: ASTNode {
    Declarator(Declaration* declaration, const Type* type, const Identifier& identifier, const Location& location);

    Declaration* declaration{};
    Linkage linkage;
    const Type* type{};
    Identifier identifier;
    Declarator* earlier{};
    Declarator* definition{};

    virtual const Type* to_type() const;
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
