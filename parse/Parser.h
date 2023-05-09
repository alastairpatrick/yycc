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
    Scope* prototype_scope{};

    const Type* apply(const Type* type);
};

struct ParseDeclaratorFlags {
    bool allow_function_definition: 1;
    bool allow_identifier: 1;
    bool allow_initializer: 1;
};

enum class SubExpressionKind {
    PRIMARY,
    POSTFIX,
    UNARY,
    CAST,
};

struct OrderIndependentScope {
    size_t position; // position after opening '{' of scope
    Scope* scope;
};

struct Parser {
    Preprocessor& preprocessor;
    IdentifierMap& identifiers;
    const bool preparse;
    vector<OrderIndependentScope> order_independent_scopes;
    unordered_set<InternedString> identifier_tokens;

    Parser(Preprocessor& preprocessor, IdentifierMap& identifiers);
    void operator=(const Parser&) = delete;

    Expr* parse_standalone_expr();  // for testing
    Statement* parse_standalone_statement();  // for testing
    bool check_eof(); // for testing

    vector<Declaration*> parse();

private:
    TokenKind token = TOK_INVALID;
    SwitchStatement* innermost_switch{};

    void consume();
    bool consume(int token);
    bool require(int token);
    bool consume_required(int token);
    void skip_unexpected();
    void unexpected_token();

    // During preparse, these skip parenthesis surrounded lexical elements instead of actually parsing.
    void balance_until(int token);
    void skip_expr(OperatorPrec min_prec);

    size_t position() const;

    void handle_declaration_directive();

    LocationNode* parse_declaration_or_statement(bool expression_valid);
    Declaration* parse_declaration(bool expression_valid);
    Declaration* parse_declaration_specifiers(bool expression_valid, const Type*& type, SpecifierSet& specifiers);
    const Type* parse_structured_type(Declaration* declaration);
    Declarator* declare_tag_type(AddScope add_scope, Declaration* declaration, const Identifier& identifier, TagType* type, const Location& location);
    const Type* parse_typeof();
    Declarator* parse_declarator(Declaration* declaration, const Type* type, SpecifierSet specifiers, ParseDeclaratorFlags flags, bool* last);
    DeclaratorTransform parse_declarator_transform(ParseDeclaratorFlags flags);
    Declarator* parse_parameter_declarator();
    Statement* parse_statement();
    CompoundStatement* parse_compound_statement();
    Expr* parse_expr(OperatorPrec min_prec, Identifier* or_label = nullptr);
    Expr* parse_sub_expr(SubExpressionKind kind, Identifier* or_label = nullptr);
    Expr* parse_initializer();
    OperatorAssoc assoc();
    OperatorPrec prec();
    const Type* parse_type(bool expression_valid);
};

#endif
