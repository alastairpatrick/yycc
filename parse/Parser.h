#ifndef PARSE_PARSER_H
#define PARSE_PARSER_H

#include "AssocPrec.h"
#include "IdentifierMap.h"
#include "preprocess/Preprocessor.h"

struct CompoundStatement;
struct Declarator;
struct Expr;
struct SwitchStatement;

struct DeclaratorTransform {
    function<const Type*(const Type*)> type_transform;
    Identifier identifier;
    vector<Declarator*> parameters;
    CompoundStatement* body{};

    const Type* apply(const Type* type);
};

enum class SubExpressionKind {
    PRIMARY,
    POSTFIX,
    UNARY,
    CAST,
};

struct Parser {
    Parser(Preprocessor& preprocessor, IdentifierMap& identifiers);
    void operator=(const Parser&) = delete;

    Expr* parse_standalone_expr();  // for testing
    Statement* parse_standalone_statement();  // for testing
    ASTNodeVector parse();
    bool check_eof();

private:
    Preprocessor& preprocessor;
    IdentifierMap& identifiers;
    TokenKind token = TOK_NUM;
    const bool preparse;
    SwitchStatement* innermost_switch{};

    void consume();
    bool consume(int t, Location* location = nullptr);
    bool consume_identifier(Identifier& identifier);
    void balance_until(int t);
    bool require(int t, Location* location = nullptr);

    void skip_unexpected();
    void unexpected_token();
    size_t position() const;
    Fragment end_fragment(size_t begin_position) const;

    void handle_declaration_directive();

    void skip_expr(OperatorPrec min_prec);

    OperatorAssoc assoc();
    OperatorPrec prec();

    Expr* parse_expr(OperatorPrec min_prec, Identifier* or_label = nullptr);
    Expr* parse_sub_expr(SubExpressionKind kind, Identifier* or_label = nullptr);
    Expr* parse_initializer();
    Declaration* parse_declaration_specifiers(IdentifierScope scope, const Type*& type, uint32_t& specifiers);
    Declaration* parse_declaration(IdentifierScope scope);
    Statement* parse_statement();
    ASTNode* parse_declaration_or_statement(IdentifierScope scope);
    CompoundStatement* parse_compound_statement();
    Declarator* parse_parameter_declarator();
    Declarator* parse_declarator(Declaration* declaration, const Type* type, uint32_t specifiers, int flags, bool* last);
    DeclaratorTransform parse_declarator_transform(IdentifierScope scope, int flags);
    Declarator* declare_tag_type(Declaration* declaration, const Identifier& identifier, TagType* type, const Location& location);
    const Type* parse_structured_type(Declaration* declaration);
    Declarator* parse_enum_constant(Declaration* declaration, const EnumType* type, Declarator* tag);
    const Type* parse_typeof();
    const Type* parse_type();
};

#endif
