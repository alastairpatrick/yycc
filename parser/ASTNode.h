#ifndef PARSER_AST_NODE_H
#define PARSER_AST_NODE_H

#include "lexer/Fragment.h"
#include "lexer/Identifier.h"
#include "lexer/Location.h"
#include "Printable.h"

struct Declaration;
struct DeclaratorDelegate;
struct Entity;
struct EnumConstant;
enum class IdentifierScope;
enum class StorageClass;
enum class Linkage;
struct Type;
struct TypeDef;
struct Value;
struct Visitor;
struct VisitDeclaratorInput;
struct VisitDeclaratorOutput;
struct VisitStatementInput;
struct VisitStatementOutput;

struct ASTNode: virtual Printable {
    ASTNode();
    ASTNode(const ASTNode&);
    ASTNode(ASTNode&&);
    ASTNode& operator=(const ASTNode&);
    ASTNode& operator=(ASTNode&&);

    ASTNode* next_delete;
};

typedef vector<ASTNode*> ASTNodeVector;

ostream& operator<<(ostream& stream, const ASTNodeVector& items);

enum class ResolutionStatus {
    UNRESOLVED,
    RESOLVING,
    RESOLVED,
};

struct Declarator: ASTNode {
    Declarator(const Declaration* declaration, const Type* type, const Identifier& identifier, const Location& location);
    Declarator(const Declaration* declaration, const Identifier& identifier, const Location& location);

    Location location;
    Fragment fragment;
    const Declaration* declaration{};
    const Type* type{};
    Identifier identifier;
    DeclaratorDelegate* delegate{};

    Declarator* next{};
    ResolutionStatus status = ResolutionStatus::UNRESOLVED;

    EnumConstant* enum_constant();
    Entity* entity();
    TypeDef* type_def();

    const Type* to_type() const;
    VisitDeclaratorOutput accept(Visitor& visitor, const VisitDeclaratorInput& input);
    void print(ostream& stream) const;
};

struct Statement: ASTNode {
    Location location;

    explicit Statement(const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) = 0;
};

struct Expr: Statement {
    explicit Expr(const Location& location);

    const Type* get_type() const;
    Value fold() const;
};


#endif
