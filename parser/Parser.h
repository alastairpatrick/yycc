#ifndef PARSER_PARSER_H
#define PARSER_PARSER_H

#include "AssocPrec.h"
#include "IdentifierMap.h"
#include "preprocessor/Preprocessor.h"

struct CompoundStatement;
struct Declarator;
struct Expr;

struct DeclaratorTransform {
    function<const Type*(const Type*)> type_transform;
    Identifier identifier;
    vector<Entity*> params;
    CompoundStatement* body{};

    const Type* apply(const Type* type);
};

struct Parser {
    Parser(Preprocessor& preprocessor, IdentifierMap& identifiers);
    void operator=(const Parser&) = delete;

    Declaration* parse_initial_directives();
    Expr* parse_standalone_expr();  // for testing
    void parse();
    bool check_eof();

    ASTNodeVector declarations;

private:
    Preprocessor& preprocessor;
    IdentifierMap& identifiers;
    TokenKind token = TOK_NUM;
    const bool preparse;

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

    Expr* parse_expr(OperatorPrec min_prec);
    Expr* parse_cast_expr();
    Expr* parse_unary_expr();
    Expr* parse_initializer();
    Declaration* parse_declaration_specifiers(IdentifierScope scope, const Type*& type, uint32_t& specifiers);
    ASTNode* parse_declaration_or_statement(IdentifierScope scope);
    CompoundStatement* parse_compound_statement();
    Declarator* parse_parameter_declarator();
    Declarator* parse_declarator(Declaration* declaration, const Type* type, uint32_t specifiers, int flags, bool* last);
    DeclaratorTransform parse_declarator_transform(IdentifierScope scope, int flags);
    const Type* parse_structured_type(Declaration* declaration);
    EnumConstant* parse_enum_constant(Declaration* declaration, const EnumType* type, const Identifier& tag);
    const Type* parse_typeof();
    const Type* parse_type_name();
};

#endif
