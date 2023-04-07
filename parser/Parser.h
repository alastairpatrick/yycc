#ifndef AST_PARSER_H
#define AST_PARSER_H

#include "AssocPrec.h"
#include "preprocessor/Preprocessor.h"
#include "SymbolMap.h"

struct CompoundStatement;
struct Declarator;
struct Expr;
struct Variable;

struct DeclaratorTransform {
    function<const Type*(const Type*)> type_transform;
    Identifier identifier;
    vector<Variable*> params;
    CompoundStatement* body{};
};

struct Parser {
    Parser(Preprocessor& preprocessor, SymbolMap& symbols);
    void operator=(const Parser&) = delete;

    Expr* parse_standalone_expr();  // for testing
    ASTNodeVector parse();
    bool check_eof();

private:
    Preprocessor& preprocessor;
    SymbolMap& symbols;
    TokenKind token = TOK_NUM;
    const bool preparse;

    void consume();
    bool consume(int t, Location* location = nullptr);
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
    Declaration* parse_declaration_specifiers(IdentifierScope scope, const Type*& type, uint32_t& specifiers);
    ASTNode* parse_declaration_or_statement(IdentifierScope scope);
    CompoundStatement* parse_compound_statement();
    Declarator* parse_parameter_declarator();
    Declarator* parse_declarator(Declaration* declaration, const Type* type, uint32_t specifiers, bool allow_function_def, const Location& location, bool* last);
    DeclaratorTransform parse_declarator_transform(bool allow_function_def);
    const Type* parse_structured_type(Declaration* declaration);
    EnumConstant* parse_enum_constant(Declaration* declaration);
};

#endif
