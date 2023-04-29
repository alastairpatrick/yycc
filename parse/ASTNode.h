#ifndef PARSE_AST_NODE_H
#define PARSE_AST_NODE_H

#include "lex/Fragment.h"
#include "lex/Identifier.h"
#include "lex/Location.h"
#include "Printable.h"

struct Declaration;
struct DeclaratorDelegate;
struct Entity;
struct EnumConstant;
struct Expr;
struct Function;
enum class IdentifierScope;
enum class Linkage;
enum class StorageClass;
struct Type;
struct TypeDef;
struct Value;
struct Variable;
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

struct LocationNode: ASTNode {
    Location location;

    LocationNode(const Location& location);
};

enum class DeclaratorStatus {
    UNRESOLVED,
    RESOLVING,
    RESOLVED,
    EMITTED,
};

struct Declarator: LocationNode {
    Declarator* primary{};
    Fragment fragment;
    const Type* type{};
    Identifier identifier;
    DeclaratorDelegate* delegate{};

    Declarator* next{};
    DeclaratorStatus status = DeclaratorStatus::UNRESOLVED;
    
    Declarator(const Declaration* declaration, const Type* type, const Identifier& identifier, const Location& location);
    Declarator(const Declaration* declaration, const Identifier& identifier, const Location& location);

    EnumConstant* enum_constant();
    Entity* entity();
    Variable* variable();
    Function* function();
    TypeDef* type_def();

    const Type* to_type() const;
    bool is_member() const;
    VisitDeclaratorOutput accept(Visitor& visitor, const VisitDeclaratorInput& input);
    void print(ostream& stream) const;

private:
    friend class DeclarationMarker;
    friend class IdentifierMap;
    const Declaration* declaration{};
};

enum class LabelKind {
    GOTO,
    CASE,
    DEFAULT,
};

struct Label {
    LabelKind kind{};
    Identifier identifier;
    Expr* case_expr{};
};

struct Statement: LocationNode {
    vector<Label> labels;

    explicit Statement(const Location& location);
    virtual VisitStatementOutput accept(Visitor& visitor, const VisitStatementInput& input) = 0;
    virtual void print(ostream& stream) const override;
};

struct Expr: Statement {
    explicit Expr(const Location& location);
    virtual bool is_null_literal() const;
};


#endif
