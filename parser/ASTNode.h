#ifndef AST_AST_NODE_H
#define AST_AST_NODE_H

#include "lexer/Fragment.h"
#include "lexer/Identifier.h"
#include "lexer/Location.h"
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
    Fragment fragment;
    ASTNode* next_delete;
};

typedef vector<ASTNode*> ASTNodeVector;

ostream& operator<<(ostream& stream, const ASTNodeVector& items);

struct Declarator: ASTNode {
    Declarator(const Declaration* declaration, const Type* type, const Identifier& identifier, const Location& location);

    const Declaration* declaration{};
    const Type* type;
    Identifier identifier;

    // During preparse, "earlier" forms a linked list of Declarators corresponding to the same entity.
    // After preparse, "earlier" is always null and there is a single Declarator instance for each entity.
    Declarator* earlier{};

    virtual const Type* to_type() const;
    virtual void compose(Declarator* later);
    virtual void print(ostream& stream) const = 0;
};

struct Statement: ASTNode {
    explicit Statement(const Location& location): ASTNode(location) {}
};

struct Expr: Statement {
    explicit Expr(const Location& location): Statement(location) {}

    virtual const Type* get_type() const = 0;
    virtual LLVMValueRef generate_value(CodeGenContext* context) const = 0;
};


#endif
