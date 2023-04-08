#ifndef AST_AST_NODE_H
#define AST_AST_NODE_H

#include "lexer/Fragment.h"
#include "lexer/Identifier.h"
#include "lexer/Location.h"
#include "Printable.h"

struct CodeGenContext;
struct Declaration;
struct DeclaratorKind;
struct EnumConstant;
struct Function;
enum class IdentifierScope;
enum class StorageClass;
enum class Linkage;
struct Type;
struct Variable;
struct TypeDef;

struct ASTNode: Printable {
    ASTNode();
    void operator=(const ASTNode&) = delete;

    ASTNode* next_delete;
};

typedef vector<ASTNode*> ASTNodeVector;

ostream& operator<<(ostream& stream, const ASTNodeVector& items);

struct Declarator: ASTNode {
    Declarator(const Declaration* declaration, const Type* type, const Identifier& identifier, const Location& location);
    Declarator(const Declaration* declaration, const Identifier& identifier, const Location& location);

    Location location;
    Fragment fragment;
    const Declaration* declaration{};
    const Type* type{};
    Identifier identifier;
    DeclaratorKind* kind{};

    // During preparse, "earlier" forms a linked list of Declarators corresponding to the same entity.
    // After preparse, "earlier" is always null and there is a single Declarator instance for each entity.
    Declarator* earlier{};

    EnumConstant* enum_constant();
    Function* function();
    Variable* variable();
    TypeDef* type_def();

    const Type* to_type() const;
    void compose(Declarator* later);
    void print(ostream& stream) const;
};

struct Statement: ASTNode {
    Location location;
    explicit Statement(const Location& location);
};

struct Expr: Statement {
    explicit Expr(const Location& location);

    virtual const Type* get_type() const = 0;
    virtual LLVMValueRef generate_value(CodeGenContext* context) const = 0;
};


#endif
