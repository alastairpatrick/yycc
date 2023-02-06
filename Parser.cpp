#include "std.h"
#include "assoc_prec.h"
#include "ASTNode.h"
#include "CompileContext.h"
#include "Decl.h"
#include "Expr.h"
#include "lex.yy.h"
#include "Statement.h"
#include "SymbolMap.h"
#include "Token.h"

struct Parser {
    Lexer lexer;
    int token;
    SymbolMap symbols;
    ASTNodeVector extern_decls;

    explicit Parser(const reflex::Input& input): lexer(symbols, input) {
        token = lexer.lex();
    }

    void consume() {
        token = lexer.lex();
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
        message(lexer.location()) << "error unexpected token\n";
        consume();
    }

    OperatorAssoc assoc() {
        return g_assoc_prec[token & 0xFF].assoc;
    }

    OperatorPrec prec() {
        return g_assoc_prec[token & 0xFF].prec;
    }

    bool check_eof() {
        if (token == TOK_EOF) return true;
        message(lexer.location()) << "Expected end of file.\n";
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
                    result = new ConditionExpr(move(result), move(then_expr), move(else_expr), loc);
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
                result = new BinaryExpr(move(result), move(right), op, loc);
            }
        }

        return result;
    }

    Expr* parse_cast_expr() {
        Expr* result{};
        Location loc;

        while (token) {
            if (token == TOK_INT_LITERAL) {
                result = new IntegerConstant(lexer.int_lit, IntegerType::of(lexer.int_signedness, lexer.int_size), loc);
                consume();
                break;
            }
            else if (token == TOK_FLOAT_LITERAL) {
                result = new FloatingPointConstant(lexer.float_lit, FloatingPointType::of(lexer.float_size), loc);
                consume();
                break;
            }
            else if (token == TOK_CHAR_LITERAL) {
                result = new IntegerConstant(lexer.int_lit, IntegerType::of_char(lexer.string_wide), loc);
                consume();
                break;
            }
            else if (token == TOK_STRING_LITERAL) {
                result = new StringConstant(move(lexer.string_lit), IntegerType::of_char(lexer.string_wide), loc);
                consume();
                break;
            }
            else if (token == TOK_IDENTIFIER) {
                result = new NameExpr(lexer.identifier, loc);
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
            message(lexer.location()) << "error unexpected end of file\n";
            result = new IntegerConstant(0, IntegerType::default_type(), lexer.location());
        }

        return result;
    }

    bool parse_decl_specifiers(StorageClass& storage_class, const Type*& type) {
        const uint32_t storage_class_mask = (1 << TOK_TYPEDEF) | (1 << TOK_EXTERN) | (1 << TOK_STATIC) | (1 << TOK_AUTO) | (1 << TOK_REGISTER);
        const uint32_t type_qualifier_mask = (1 << TOK_CONST) | (1 << TOK_RESTRICT) | (1 << TOK_VOLATILE);
        const uint32_t type_specifier_mask = ~(storage_class_mask | type_qualifier_mask);

        Location storage_class_location;
        Location type_specifier_location;
        Location qualifier_location;
        uint32_t specifier_set = 0;
        uint32_t qualifier_set = 0;
        int num_longs = 0;
        bool is_declaration = false;
        while (token) {
            if (token == TOK_LONG) {
                ++num_longs;
                if (num_longs > 2) {
                    message(lexer.location()) << "error invalid type specifier combination\n";
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

            case TOK_TYPE_IDENTIFIER:
                if ((specifier_set & type_specifier_mask) == 0) {
                    should_consume = true;
                    type_specifier_location = lexer.location();                
                    type = symbols.lookup_type(TypeNameKind::ORDINARY, lexer.identifier);
                }

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
                    message(lexer.location()) << "error invalid declaration specifier or type qualifier combination\n";
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
            message(storage_class_location) << "error too many storage classes\n";
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
        case (1 << TOK_TYPE_IDENTIFIER):
            assert(type);
            break;
        default:
            type = IntegerType::default_type();
            message(type_specifier_location) << "error invalid type specifier combination\n";
            break;
        }

        if (specifier_set & type_qualifier_mask) {
            type = QualifiedType::of(type, specifier_set & type_qualifier_mask);
        }

        return true;
    }

    // This parses a single declaration or a single statement. It can append multiple items to the
    // statement list when it flattens a single declaration with multiple declarators into multiple
    // declarations, each with a single declarator. The AST has no notion of declarators.
    void parse_decl_or_statement(ASTNodeVector& list) {
        auto location = lexer.location();

        StorageClass storage_class = StorageClass::NONE;
        const Type* type{};
        if (parse_decl_specifiers(storage_class, type)) {
            int decl_count = 0;
            while (token && token != ';') {
                auto decl = parse_declarator(storage_class, type, decl_count == 0, location);
                ++ decl_count;

                auto is_function_definition = decl->is_function_definition();
                if (decl->identifier == empty_string()) {
                    message(lexer.location()) << "error expected identifier\n";
                } else if (!decl->redundant) {
                    if (decl->storage_class != StorageClass::EXTERN) {
                        list.push_back(decl);
                    } else {
                        extern_decls.push_back(decl);
                    }
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

            list.push_back(move(decl));
        }
    }

    CompoundStatement* parse_compound_statement() {
        symbols.push_scope();

        Location loc;
        require('{', &loc);

        ASTNodeVector list;
        while (token && token != '}') {
            parse_decl_or_statement(list);
        }

        // Must pop scope before consuming '}' in case '}' is immediately followed by an identifier that the
        // lexer must correctly identify as TOK_IDENTIFIER or TOK_TYPE_IDENTIFIER.
        symbols.pop_scope();

        require('}');

        return new CompoundStatement(move(list), loc);
    }

    Decl* parse_parameter_decl() {
        auto location = lexer.location();
        StorageClass storage_class = StorageClass::NONE;
        const Type* type{};
        if (!parse_decl_specifiers(storage_class, type)) {
            // TODO
        }

        if (storage_class != StorageClass::NONE) {
            message(location) << "error a parameter may not have a storage class\n";
            storage_class = StorageClass::NONE;
        }

        return parse_declarator(storage_class, type, false, location);        
    }

    Decl* parse_declarator(StorageClass storage_class, const Type* declaration_type, bool allow_function_def, const Location& location) {
        if (consume('(')) {
            auto result = parse_declarator(storage_class, declaration_type, allow_function_def, location);
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

            const string* identifier = lexer.identifier;
            if (!consume(TOK_IDENTIFIER) && !consume(TOK_TYPE_IDENTIFIER)) identifier = empty_string();

            Expr* initializer{};
            if (consume('=')) {
                initializer = parse_expr(ASSIGN_PREC);
            }

            Decl* decl{};

            if (consume('(')) {
                symbols.push_scope();

                vector<Variable*> params;
                vector<const Type*> param_types;
                bool seen_void = false;
                while (token && !consume(')')) {
                    auto decl = parse_parameter_decl();
                    params.push_back(dynamic_cast<Variable*>(decl));  // TODO: what if it's not a variable!

                    if (decl->type == &VoidType::it) {
                        if (seen_void || !param_types.empty()) {
                            message(decl->location) << "error a parameter may not have void type\n";
                        }
                        seen_void = true;
                    } else {
                        param_types.push_back(decl->type);
                    }

                    consume(',');
                }

                type = FunctionType::of(type, move(param_types), false);

                if (storage_class != StorageClass::TYPEDEF) {
                    decl = new Function(storage_class,
                                        static_cast<const FunctionType*>(type),
                                        identifier,
                                        move(params),
                                        allow_function_def && token == '{' ? parse_compound_statement() : nullptr,
                                        location);
                }

                symbols.pop_scope();
            }

            if (storage_class == StorageClass::TYPEDEF) {
                decl = new TypeDef(type, identifier, location);
            }

            if (!decl) {
                decl = new Variable(storage_class, type, identifier, move(initializer), location);
            }

            if (identifier != empty_string()) {
                symbols.add_decl(TypeNameKind::ORDINARY, decl->identifier, decl);
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
        parser.parse_decl_or_statement(ast);
    }
    parser.insert_externs(ast);

    return ast;
}