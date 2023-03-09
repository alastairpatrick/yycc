#include "assoc_prec.h"
#include "ASTNode.h"
#include "CompileContext.h"
#include "Constant.h"
#include "Decl.h"
#include "Expr.h"
#include "Statement.h"
#include "SymbolMap.h"
#include "Token.h"
#include "preprocessor/TokenConverter.h"

struct Parser {
    TokenConverter lexer;
    TokenKind token;
    SymbolMap symbols;
    ASTNodeVector extern_decls;

    explicit Parser(const reflex::Input& input): lexer(input) {
        token = TokenKind(lexer.next_token());
    }

    void consume() {
        token = TokenKind(lexer.next_token());
    }

    bool consume(int t, Location* location = nullptr) {
        if (token != t) return false;
        if (location) {
            *location = lexer.location();
        }
        consume();
        return true;
    }

    bool require(int t, Location* location = nullptr) {
        while (token && token != t) {
            skip();
        }
        if (token == t) {
            if (location) {
                *location = lexer.location();
            }
            consume();
            return true;
        }
        return false;
    }

    void skip() {
        message(Severity::ERROR, lexer.location()) << "unexpected token\n";
        consume();
    }

    OperatorAssoc assoc() {
        assert(token >= 0 && token < TOK_NUM);
        return g_assoc_prec[token].assoc;
    }

    OperatorPrec prec() {
        assert(token >= 0 && token < TOK_NUM);
        return g_assoc_prec[token].prec;
    }

    bool check_eof() {
        if (token == TOK_EOF) return true;
        message(Severity::ERROR, lexer.location()) << "expected end of file\n";
        return false;
    }

    Expr* parse_expr(int min_prec) {
        Location loc;
        auto result = parse_cast_expr();

        while (prec() >= min_prec) {
            auto next_min_prec = assoc() == ASSOC_LEFT ? prec() + 1 : prec();

            if (consume('?', &loc)) {
                auto then_expr = parse_expr(next_min_prec);
                if (require(':')) {
                    auto else_expr = parse_expr(CONDITIONAL_PREC);
                    result = new ConditionExpr(result, then_expr, else_expr, loc);
                }
            }
            else {
                BinaryOp op;
                switch (token) {
                case TOK_OR_OP:
                case TOK_AND_OP:
                case '+':
                case '-':
                case '*':
                case '/':
                case '%':
                    loc = lexer.location();
                    op = BinaryOp(token);
                    consume();
                    break;
                default:
                    return result;
                }

                auto right = parse_expr(next_min_prec);
                result = new BinaryExpr(result, right, op, loc);
            }
        }

        return result;
    }

    Expr* parse_cast_expr() {
        Expr* result{};

        while (token) {
            if (token == TOK_BIN_INT_LITERAL || token == TOK_OCT_INT_LITERAL || token == TOK_DEC_INT_LITERAL || token == TOK_HEX_INT_LITERAL || token == TOK_CHAR_LITERAL) {
                result = IntegerConstant::of(lexer.text(), token, lexer.location());
                consume();
                break;
            }
            else if (token == TOK_DEC_FLOAT_LITERAL || token == TOK_HEX_FLOAT_LITERAL) {
                result = FloatingPointConstant::of(lexer.text(), token, lexer.location());
                consume();
                break;
            }
            else if (token == TOK_STRING_LITERAL) {
                result = StringConstant::of(lexer.text(), lexer.location());
                consume();
                break;
            }
            else if (token == TOK_IDENTIFIER) {
                Decl* decl = symbols.lookup_decl(TypeNameKind::ORDINARY, lexer.identifier());
                if (decl) {
                    // TokenKind would have to be TOK_TYPEDEF_IDENTIFIER for decl to be a typedef.
                    assert(!dynamic_cast<TypeDef*>(decl));

                    result = new NameExpr(decl, lexer.location());
                } else {
                    message(Severity::ERROR, lexer.location()) << '\'' << lexer.identifier() << "' undeclared\n";
                    result = new IntegerConstant(0, IntegerType::default_type(), lexer.location());
                }
                consume();
                break;
            } else if (consume('(')) {
                result = parse_expr(0);
                require(')');
            }
            else {
                skip();
            }
        }

        if (!result) {
            assert(token == TOK_EOF);
            message(Severity::ERROR, lexer.location()) << "unexpected end of file\n";
            result = new IntegerConstant(0, IntegerType::default_type(), lexer.location());
        }

        return result;
    }

    bool parse_decl_specifiers(StorageClass& storage_class, const Type*& type, uint32_t& specifiers) {
        const uint32_t storage_class_mask = (1 << TOK_TYPEDEF) | (1 << TOK_EXTERN) | (1 << TOK_STATIC) | (1 << TOK_AUTO) | (1 << TOK_REGISTER);
        const uint32_t type_qualifier_mask = (1 << TOK_CONST) | (1 << TOK_RESTRICT) | (1 << TOK_VOLATILE);
        const uint32_t function_specifier_mask = 1 << TOK_INLINE;
        const uint32_t type_specifier_mask = ~(storage_class_mask | type_qualifier_mask | function_specifier_mask);

        Location storage_class_location;
        Location type_specifier_location;
        Location function_specifier_location;
        Location qualifier_location;
        uint32_t specifier_set = 0;
        uint32_t qualifier_set = 0;
        int num_longs = 0;
        bool is_declaration = false;
        while (token) {
            if (token == TOK_LONG) {
                ++num_longs;
                if (num_longs > 2) {
                    message(Severity::ERROR, lexer.location()) << "invalid type specifier combination\n";
                }
                specifier_set &= ~(1 << token);
            }

            bool should_consume = false;
            switch (token) {
            case TOK_TYPEDEF:
            case TOK_EXTERN:
            case TOK_STATIC:
            case TOK_AUTO:
            case TOK_REGISTER:
                should_consume = true;
                storage_class_location = lexer.location();
                break;

            case TOK_VOID:
            case TOK_CHAR:
            case TOK_SHORT:
            case TOK_INT:
            case TOK_LONG:
            case TOK_FLOAT:
            case TOK_DOUBLE:
            case TOK_SIGNED:
            case TOK_UNSIGNED:
            case TOK_BOOL:
            case TOK_COMPLEX:
                should_consume = true;
                type_specifier_location = lexer.location();                
                break;

            case TOK_IDENTIFIER: {
                    auto typedef_type = symbols.lookup_type(TypeNameKind::ORDINARY, lexer.identifier());
                    if (!typedef_type) break;

                    if ((specifier_set & type_specifier_mask) == 0) {
                        type = typedef_type;
                        should_consume = true;
                        type_specifier_location = lexer.location();                
                    }
                }
                break;

            case TOK_INLINE:
                should_consume = true;
                function_specifier_location = lexer.location();
                specifier_set &= ~(1 << token); // function speficiers may be repeated
                break;

            case TOK_CONST:
            case TOK_RESTRICT:
            case TOK_VOLATILE:
                should_consume = true;
                qualifier_location = lexer.location();
                specifier_set &= ~(1 << token); // qualifiers may be repeated
                break;
            }

            if (should_consume) {
                assert(token < 32);
                is_declaration = true;
                if (specifier_set & (1 << token)) {
                    message(Severity::ERROR, lexer.location()) << "invalid declaration specifier or type qualifier combination\n";
                }
                specifier_set |= 1 << token;
                consume();
            } else {
                break;
            }
        }

        if (!is_declaration) return false;

        // Have single storage class specifier iff storage class set contains only one element.
        uint32_t storage_class_set = specifier_set & storage_class_mask;
        if (storage_class_set != 0 && storage_class_set & (storage_class_set-1)) {
            message(Severity::ERROR, storage_class_location) << "too many storage classes\n";
        }

        storage_class = StorageClass::NONE;
        if (storage_class_set & (1 << TOK_STATIC)) storage_class = StorageClass::STATIC;
        else if (storage_class_set & (1 << TOK_EXTERN)) storage_class = StorageClass::EXTERN;
        else if (storage_class_set & (1 << TOK_TYPEDEF)) storage_class = StorageClass::TYPEDEF;
        else if (storage_class_set & (1 << TOK_AUTO)) storage_class = StorageClass::AUTO;
        else if (storage_class_set & (1 << TOK_REGISTER)) storage_class = StorageClass::REGISTER;

        // Check type specifiers are one of the valid combinations.
        switch (specifier_set & type_specifier_mask) {
        case (1 << TOK_VOID):
            type = &VoidType::it;
            break;
        case (1 << TOK_CHAR):
            type = IntegerType::of_char(false);
            break;
        case (1 << TOK_SIGNED) | (1 << TOK_CHAR):
            type = IntegerType::of(IntegerSignedness::SIGNED, IntegerSize::CHAR);
            break;
        case (1 << TOK_UNSIGNED) | (1 << TOK_CHAR):
            type = IntegerType::of(IntegerSignedness::UNSIGNED, IntegerSize::CHAR);
            break;
        case (1 << TOK_SHORT):
        case (1 << TOK_SIGNED) | (1 << TOK_SHORT):
        case (1 << TOK_SHORT) | (1 << TOK_INT):
        case (1 << TOK_SIGNED) | (1 << TOK_SHORT) | (1 << TOK_INT):
            type = IntegerType::of(IntegerSignedness::SIGNED, IntegerSize::SHORT);
            break;
        case (1 << TOK_UNSIGNED) | (1 << TOK_SHORT):
        case (1 << TOK_UNSIGNED) | (1 << TOK_SHORT) | (1 << TOK_INT):
            type = IntegerType::of(IntegerSignedness::UNSIGNED, IntegerSize::SHORT);
            break;
        case 0:
        case (1 << TOK_INT):
        case (1 << TOK_SIGNED):
        case (1 << TOK_SIGNED) | (1 << TOK_INT):
            type = IntegerType::of(IntegerSignedness::SIGNED, IntegerSize::INT);
            break;
        case (1 << TOK_UNSIGNED):
        case (1 << TOK_UNSIGNED) | (1 << TOK_INT):
            type = IntegerType::of(IntegerSignedness::UNSIGNED, IntegerSize::INT);
            break;
        case (1 << TOK_LONG):
        case (1 << TOK_SIGNED) | (1 << TOK_LONG):
        case (1 << TOK_LONG) | (1 << TOK_INT):
        case (1 << TOK_SIGNED) | (1 << TOK_LONG) | (1 << TOK_INT):
            type = IntegerType::of(IntegerSignedness::SIGNED, num_longs == 1 ? IntegerSize::LONG : IntegerSize::LONG_LONG);
            break;
        case (1 << TOK_UNSIGNED) | (1 << TOK_LONG):
        case (1 << TOK_UNSIGNED) | (1 << TOK_LONG) | (1 << TOK_INT):
            type = IntegerType::of(IntegerSignedness::UNSIGNED, num_longs == 1 ? IntegerSize::LONG : IntegerSize::LONG_LONG);
            break;
        case (1 << TOK_FLOAT):
            type = FloatingPointType::of(FloatingPointSize::FLOAT);
            break;
        case (1 << TOK_DOUBLE):
            type = FloatingPointType::of(FloatingPointSize::DOUBLE);
            break;
        case (1 << TOK_LONG) | (1 << TOK_DOUBLE):
            type = FloatingPointType::of(FloatingPointSize::LONG_DOUBLE);
            break;
        case (1 << TOK_BOOL):
            type = IntegerType::of(IntegerSignedness::UNSIGNED, IntegerSize::BOOL);
            break;
        case (1 << TOK_IDENTIFIER):
            break;
        default:
            type = IntegerType::default_type();
            message(Severity::ERROR, type_specifier_location) << "invalid type specifier combination\n";
            break;
        }

        if (specifier_set & type_qualifier_mask) {
            type = QualifiedType::of(type, specifier_set & type_qualifier_mask);
        }

        specifiers = specifier_set & function_specifier_mask;
        return true;
    }

    // This parses a single declaration or a single statement. It can append multiple items to the
    // statement list when it flattens a single declaration with multiple declarators into multiple
    // declarations, each with a single declarator. The AST has no notion of declarators.
    void parse_decl_or_statement(IdentifierScope scope, ASTNodeVector& list) {
        auto location = lexer.location();

        StorageClass storage_class = StorageClass::NONE;
        const Type* type{};
        uint32_t specifiers;
        if (parse_decl_specifiers(storage_class, type, specifiers)) {
            int decl_count = 0;
            while (token && token != ';') {
                auto decl = parse_declarator(scope, storage_class, type, specifiers, decl_count == 0, location);
                ++ decl_count;

                auto is_function_definition = decl->is_function_definition();
                if (decl->identifier.name->empty()) {
                    message(Severity::ERROR, lexer.location()) << "expected identifier\n";
                } else {
                    list.push_back(decl);
                }

                // No ';' after function definition.
                if (is_function_definition) return;

                if (!consume(',')) break;
            }

            require(';');
        } else {
            ASTNode* decl{};
            if (token == '{') {
                decl = parse_compound_statement();
            } else if (consume(TOK_RETURN)) {
                decl = new ReturnStatement(parse_expr(0), location);
                require(';');
            } else {
                decl = parse_expr(0);
                require(';');
            }

            list.push_back(decl);
        }
    }

    CompoundStatement* parse_compound_statement() {
        symbols.push_scope();

        Location loc;
        require('{', &loc);

        ASTNodeVector list;
        while (token && token != '}') {
            parse_decl_or_statement(IdentifierScope::BLOCK, list);
        }

        // Must pop scope before consuming '}' in case '}' is immediately followed by an identifier that the
        // lexer must correctly identify as TOK_IDENTIFIER or TOK_TYPEDEF_IDENTIFIER.
        symbols.pop_scope();

        require('}');

        return new CompoundStatement(move(list), loc);
    }

    Decl* parse_parameter_decl() {
        auto location = lexer.location();
        StorageClass storage_class = StorageClass::NONE;
        const Type* type{};
        uint32_t specifiers;
        if (!parse_decl_specifiers(storage_class, type, specifiers)) {
            // TODO
        }

        if (storage_class != StorageClass::NONE && storage_class != StorageClass::REGISTER) {
            message(Severity::ERROR, location) << "invalid storage class\n";
            storage_class = StorageClass::NONE;
        }

        return parse_declarator(IdentifierScope::PROTOTYPE, storage_class, type, specifiers, false, location);        
    }

    Decl* parse_declarator(IdentifierScope scope, StorageClass storage_class, const Type* declaration_type, uint32_t specifiers, bool allow_function_def, const Location& location) {
        if (consume('(')) {
            auto result = parse_declarator(scope, storage_class, declaration_type, specifiers, allow_function_def, location);
            require(')');
            return result;
        } else {
            const Type* type = declaration_type;
            while (consume('*')) {
                type = type->pointer_to();

                unsigned qualifier_set = 0;
                while (token == TOK_CONST || token == TOK_RESTRICT || token == TOK_VOLATILE) {
                    qualifier_set |= 1 << token;
                    consume();
                }

                if (qualifier_set) {
                    type = QualifiedType::of(type, qualifier_set);
                }
            }

            auto identifier = lexer.identifier();
            if (!consume(TOK_IDENTIFIER)) identifier.name = EmptyInternedString;

            Expr* initializer{};
            Decl* decl{};

            if (consume('=')) {
                initializer = parse_expr(ASSIGN_PREC);
            } else if (consume('(')) {
                symbols.push_scope();

                vector<Variable*> params;
                vector<const Type*> param_types;
                bool seen_void = false;
                while (token && !consume(')')) {
                    auto decl = parse_parameter_decl();

                    // Functions are adjusted to variable of function pointer type.
                    auto variable = dynamic_cast<Variable*>(decl);
                    assert(variable);

                    params.push_back(variable);

                    if (decl->type == &VoidType::it) {
                        if (seen_void || !param_types.empty()) {
                            message(Severity::ERROR, decl->location) << "a parameter may not have void type\n";
                        }
                        seen_void = true;
                    } else {
                        param_types.push_back(decl->type);
                    }
                    consume(',');
                }

                type = FunctionType::of(type, move(param_types), false);

                if (scope == IdentifierScope::PROTOTYPE) {
                    type = type->pointer_to();
                } else if (storage_class != StorageClass::TYPEDEF) {
                    decl = new Function(scope,
                                        storage_class,
                                        static_cast<const FunctionType*>(type),
                                        specifiers,
                                        identifier,
                                        move(params),
                                        allow_function_def && token == '{' ? parse_compound_statement() : nullptr,
                                        location);
                }

                symbols.pop_scope();
            }

            if (!decl && (specifiers & (1 << TOK_INLINE))) {
                message(Severity::ERROR, location) << "'inline' may only appear on function\n";
            }

            if (storage_class == StorageClass::TYPEDEF) {
                decl = new TypeDef(scope, type, identifier, location);
            }

            if (!decl) {
                if (!initializer && storage_class != StorageClass::EXTERN) {
                    initializer = new DefaultExpr(type, location);
                }
                decl = new Variable(scope, storage_class, type, identifier, initializer, location);
            }

            if (!identifier.name->empty()) {
                symbols.add_decl(TypeNameKind::ORDINARY, decl);
            }

            return decl;
        }
    }

    void insert_externs(ASTNodeVector& ast) {
        ast.insert(ast.begin(), extern_decls.begin(), extern_decls.end());
        extern_decls.clear();
    }
};

Expr* parse_expr(const string& input) {
    Parser parser(input);
    auto result = parser.parse_expr(0);
    if (!parser.check_eof()) return nullptr;
    return result;
}

ASTNodeVector parse_statements(const string& input) {
    ASTNodeVector ast;
    Parser parser(input);
    while (parser.token != 0) {
        parser.parse_decl_or_statement(IdentifierScope::FILE, ast);
    }
    parser.insert_externs(ast);

    return ast;
}