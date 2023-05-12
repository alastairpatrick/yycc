#ifndef PARSE_AST_NODE_H
#define PARSE_AST_NODE_H

#include "lex/Location.h"
#include "parse/Identifier.h"
#include "Printable.h"

struct Declaration;
struct DeclaratorDelegate;
struct Entity;
struct EnumConstant;
struct Expr;
struct Function;
enum class Linkage;
struct Scope;
enum class ScopeKind;
enum class StorageClass;
struct Type;
struct TypeDelegate;
struct Value;
struct Variable;
struct Visitor;
struct VisitDeclaratorInput;
struct VisitDeclaratorOutput;
struct VisitExpressionOutput;
struct VisitStatementOutput;

struct ASTNode {
    ASTNode* next_delete;

    ASTNode();
    virtual ~ASTNode() = default;
    void operator=(const ASTNode&) = delete;
};

struct LocationNode: ASTNode, virtual Printable {
    Location location;

    LocationNode(const Location& location);
};

typedef vector<LocationNode*> ASTNodeVector;

ostream& operator<<(ostream& stream, const ASTNodeVector& items);

enum class DeclaratorStatus {
    UNRESOLVED,
    RESOLVING,
    RESOLVED,
    EMITTED,
};

struct Declarator: LocationNode {
    Declarator* primary{};
    Declarator* next{};
    const Type* type{};
    Scope* scope{};
    InternedString identifier;
    size_t identifier_position{};
    DeclaratorDelegate* delegate{};
    DeclaratorStatus status = DeclaratorStatus::UNRESOLVED;
    
    Declarator(const Declaration* declaration, const Type* type, InternedString identifier, DeclaratorDelegate* delegate, const Location& location);

    EnumConstant* enum_constant();
    Entity* entity() const;
    Variable* variable() const;
    Function* function() const;
    TypeDelegate* type_delegate() const;

    const Type* to_type() const;
    const char* message_kind() const;
    bool is_member() const;
    void message_see_declaration(const char* declaration_kind = nullptr) const;
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
    virtual VisitStatementOutput accept(Visitor& visitor) = 0;
    virtual void print(ostream& stream) const override;
};

struct Expr: LocationNode {
    explicit Expr(const Location& location);
    virtual VisitExpressionOutput accept(Visitor& visitor) = 0;
};


#endif
