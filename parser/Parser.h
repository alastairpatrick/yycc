#ifndef AST_PARSER_H
#define AST_PARSER_H

#include "assoc_prec.h"
#include "preprocessor/Preprocessor2.h"
#include "SymbolMap.h"

struct CompoundStatement;
struct Declarator;
struct Expr;

struct Parser {
    ASTNodeVector declarations;
    SymbolMap symbols;

    Parser(const Input& input, bool preparse);
    Parser(string_view input, bool preparse);
    void operator=(const Parser&) = delete;

    Expr* parse_expr(int min_prec);
    void parse_unit();
    bool check_eof();

private:
    Preprocessor2 preprocessor;
    TokenKind token;
    const bool preparse;

    void consume();
    bool consume(int t, Location* location = nullptr);
    bool require(int t, Location* location = nullptr);
    void skip();
    size_t position() const;
    Fragment end_fragment(size_t begin_position) const;
    OperatorAssoc assoc();
    OperatorPrec prec();

    Expr* parse_cast_expr();
    bool parse_declaration_specifiers(IdentifierScope scope, StorageClass& storage_class, const Type*& type, uint32_t& specifiers);
    ASTNode* parse_declaration_or_statement(IdentifierScope scope);
    CompoundStatement* parse_compound_statement();
    Declarator* parse_parameter_declarator();
    Declarator* parse_declarator(Declaration* declaration, uint32_t specifiers, bool allow_function_def, const Location& location, bool* last);
};

#endif
