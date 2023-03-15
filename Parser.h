#ifndef PARSER_H
#define PARSER_H

#include "assoc_prec.h"
#include "preprocessor/TokenConverter.h"
#include "SymbolMap.h"

struct CompoundStatement;
struct Declarator;
struct Expr;

struct Parser {
    SymbolMap symbols;

    Parser(const Input& input, bool preparse);
    void operator=(const Parser&) = delete;

    Expr* parse_expr(int min_prec);
    ASTNode* parse_declaration_or_statement(IdentifierScope scope);
    bool is_eof();
    bool check_eof();

private:
    TokenConverter lexer;
    TokenKind token;
    const bool preparse;

    void consume();
    bool consume(int t, Location* location = nullptr);
    bool require(int t, Location* location = nullptr);
    void skip();
    const char* data() const;
    string_view end_text(const char* begin) const;
    OperatorAssoc assoc();
    OperatorPrec prec();

    Expr* parse_cast_expr();
    bool parse_declaration_specifiers(IdentifierScope scope, StorageClass& storage_class, const Type*& type, uint32_t& specifiers);
    CompoundStatement* parse_compound_statement();
    Declarator* parse_parameter_declarator();
    Declarator* parse_declarator(Declaration* declaration, uint32_t specifiers, bool allow_function_def, const Location& location, bool* last);
};

#endif
