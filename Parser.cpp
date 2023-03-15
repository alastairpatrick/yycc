#include "Parser.h"

#include "ASTNode.h"
#include "Constant.h"
#include "Declaration.h"
#include "Expr.h"
#include "Message.h"
#include "Statement.h"
#include "Token.h"

Parser::Parser(const Input& input, bool preparse): lexer(input), preparse(preparse), symbols(preparse) {
    token = TokenKind(lexer.next_token());
}

void Parser::consume() {
    token = TokenKind(lexer.next_token());
}

bool Parser::consume(int t, Location* location) {
    if (token != t) return false;
    if (location) {
        *location = lexer.location();
    }
    consume();
    return true;
}

bool Parser::require(int t, Location* location) {
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

void Parser::skip() {
    message(Severity::ERROR, lexer.location()) << "unexpected token\n";
    consume();
}

const char* Parser::data() const {
    return lexer.text().data();
}

string_view Parser::end_text(const char* begin) const {
    return string_view(begin, data() - begin);
}

OperatorAssoc Parser::assoc() {
    assert(token >= 0 && token < TOK_NUM);
    return g_assoc_prec[token].assoc;
}

OperatorPrec Parser::prec() {
    assert(token >= 0 && token < TOK_NUM);
    return g_assoc_prec[token].prec;
}

bool Parser::is_eof() {
    return token == TOK_EOF;
}

bool Parser::check_eof() {
    if (token == TOK_EOF) return true;
    message(Severity::ERROR, lexer.location()) << "expected end of file\n";
    return false;
}

Expr* Parser::parse_expr(int min_prec) {
    Location loc;
    auto begin_text = lexer.text().data();

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

        result->text = end_text(begin_text);
    }

    return result;
}

Expr* Parser::parse_cast_expr() {
    Expr* result{};
    auto begin_text = data();

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
            Declarator* declarator = symbols.lookup_declarator(false, lexer.identifier());
            if (declarator) {
                // TokenKind would have to be TOK_TYPEDEF_IDENTIFIER for declarator to be a typedef.
                assert(!dynamic_cast<TypeDef*>(declarator));

                result = new NameExpr(declarator, lexer.location());
            } else {
                message(Severity::ERROR, lexer.location()) << '\'' << lexer.identifier() << "' undeclared\n";
                result = IntegerConstant::default_expr(lexer.location());
            }
            consume();
            break;
        } else if (consume('(')) {
            result = parse_expr(0);
            require(')');
        }
        else {
            skip();
            begin_text = data();
        }
    }

    if (!result) {
        assert(token == TOK_EOF);
        message(Severity::ERROR, lexer.location()) << "unexpected end of file\n";
        result = IntegerConstant::default_expr(lexer.location());
    }

    result->text = end_text(begin_text);
    return result;
}

bool Parser::parse_declaration_specifiers(IdentifierScope scope, StorageClass& storage_class, const Type*& type, uint32_t& specifiers) {
    const uint32_t storage_class_mask = (1 << TOK_TYPEDEF) | (1 << TOK_EXTERN) | (1 << TOK_STATIC) | (1 << TOK_AUTO) | (1 << TOK_REGISTER);
    const uint32_t type_qualifier_mask = (1 << TOK_CONST) | (1 << TOK_RESTRICT) | (1 << TOK_VOLATILE);
    const uint32_t function_specifier_mask = 1 << TOK_INLINE;
    const uint32_t type_specifier_mask = ~(storage_class_mask | type_qualifier_mask | function_specifier_mask);

    Location storage_class_location;
    Location type_specifier_location = lexer.location();
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
            case TOK_REGISTER: {
              should_consume = true;
              storage_class_location = lexer.location();
              break;

          } case TOK_VOID:
            case TOK_CHAR:
            case TOK_SHORT:
            case TOK_INT:
            case TOK_LONG:
            case TOK_FLOAT:
            case TOK_DOUBLE:
            case TOK_SIGNED:
            case TOK_UNSIGNED:
            case TOK_BOOL:
            case TOK_COMPLEX: {
              should_consume = true;
              type_specifier_location = lexer.location();                
              break;

            } case TOK_IDENTIFIER: {
              if ((specifier_set & type_specifier_mask) == 0) {
                  const Type* typedef_type;
                  typedef_type = symbols.lookup_type(false, lexer.identifier());
                  if (!typedef_type) {
                      if (scope != IdentifierScope::FILE) break;

                      if (preparse) {
                          typedef_type = NamedType::of(TypeNameKind::ORDINARY, lexer.identifier());
                      } else {
                          message(Severity::ERROR, lexer.location()) << "typedef \'" << lexer.identifier() << "' undefined\n";
                          typedef_type = IntegerType::default_type();
                      }
                  }

                  type = typedef_type;
                  should_consume = true;
                  type_specifier_location = lexer.location();                
              }
              break;

          } case TOK_INLINE: {
              should_consume = true;
              function_specifier_location = lexer.location();
              specifier_set &= ~(1 << token); // function speficiers may be repeated
              break;

          } case TOK_CONST:
            case TOK_RESTRICT:
            case TOK_VOLATILE: {
              should_consume = true;
              qualifier_location = lexer.location();
              specifier_set &= ~(1 << token); // qualifiers may be repeated
              break;
          }
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
        default: {
          type = IntegerType::default_type();
          message(Severity::ERROR, type_specifier_location) << "invalid type specifier combination\n";
          break;

      } case (1 << TOK_VOID): {
          type = &VoidType::it;
          break;

      } case (1 << TOK_CHAR): {
          type = IntegerType::of_char(false);
          break;

      } case (1 << TOK_SIGNED) | (1 << TOK_CHAR): {
          type = IntegerType::of(IntegerSignedness::SIGNED, IntegerSize::CHAR);
          break;

      } case (1 << TOK_UNSIGNED) | (1 << TOK_CHAR): {
          type = IntegerType::of(IntegerSignedness::UNSIGNED, IntegerSize::CHAR);
          break;

      } case (1 << TOK_SHORT):
        case (1 << TOK_SIGNED) | (1 << TOK_SHORT):
        case (1 << TOK_SHORT) | (1 << TOK_INT):
        case (1 << TOK_SIGNED) | (1 << TOK_SHORT) | (1 << TOK_INT): {
          type = IntegerType::of(IntegerSignedness::SIGNED, IntegerSize::SHORT);
          break;

      } case (1 << TOK_UNSIGNED) | (1 << TOK_SHORT):
        case (1 << TOK_UNSIGNED) | (1 << TOK_SHORT) | (1 << TOK_INT): {
          type = IntegerType::of(IntegerSignedness::UNSIGNED, IntegerSize::SHORT);
          break;

      } case (1 << TOK_INT):
        case (1 << TOK_SIGNED):
        case (1 << TOK_SIGNED) | (1 << TOK_INT): {
          type = IntegerType::of(IntegerSignedness::SIGNED, IntegerSize::INT);
          break;

      } case (1 << TOK_UNSIGNED):
        case (1 << TOK_UNSIGNED) | (1 << TOK_INT): {
          type = IntegerType::of(IntegerSignedness::UNSIGNED, IntegerSize::INT);
          break;

      } case (1 << TOK_LONG):
        case (1 << TOK_SIGNED) | (1 << TOK_LONG):
        case (1 << TOK_LONG) | (1 << TOK_INT):
        case (1 << TOK_SIGNED) | (1 << TOK_LONG) | (1 << TOK_INT): {
          type = IntegerType::of(IntegerSignedness::SIGNED, num_longs == 1 ? IntegerSize::LONG : IntegerSize::LONG_LONG);
          break;

      } case (1 << TOK_UNSIGNED) | (1 << TOK_LONG):
        case (1 << TOK_UNSIGNED) | (1 << TOK_LONG) | (1 << TOK_INT): {
          type = IntegerType::of(IntegerSignedness::UNSIGNED, num_longs == 1 ? IntegerSize::LONG : IntegerSize::LONG_LONG);
          break;

      } case (1 << TOK_FLOAT): {
          type = FloatingPointType::of(FloatingPointSize::FLOAT);
          break;

      } case (1 << TOK_DOUBLE): {
          type = FloatingPointType::of(FloatingPointSize::DOUBLE);
          break;

      } case (1 << TOK_LONG) | (1 << TOK_DOUBLE): {
          type = FloatingPointType::of(FloatingPointSize::LONG_DOUBLE);
          break;

      } case (1 << TOK_BOOL): {
          type = IntegerType::of(IntegerSignedness::UNSIGNED, IntegerSize::BOOL);
          break;

      } case (1 << TOK_IDENTIFIER): {
          break;
      }
    }

    if (specifier_set & type_qualifier_mask) {
        type = QualifiedType::of(type, specifier_set & type_qualifier_mask);
    }

    specifiers = specifier_set & function_specifier_mask;
    return true;
}

void Parser::parse_declaration_or_statement(IdentifierScope scope, ASTNodeVector& list) {
    auto location = lexer.location();
    auto begin_text = data();

    StorageClass storage_class = StorageClass::NONE;
    const Type* type{};
    uint32_t specifiers;
    if (parse_declaration_specifiers(scope, storage_class, type, specifiers)) {
        auto declaration = new Declaration(scope, storage_class, type, location);;
            
        int declarator_count = 0;
        bool last_declarator{};
        while (token && token != ';') {
            auto declarator = parse_declarator(declaration, specifiers, declarator_count == 0, location, &last_declarator);

            if (declarator->identifier.name->empty()) {
                message(Severity::ERROR, lexer.location()) << "expected identifier\n";
            } else {
                declaration->declarators.push_back(declarator);
                if (declarator_count == 0) {
                    list.push_back(declaration);
                }
                ++declarator_count;
            }

            // No ';' or ',' after function definition.
            if (last_declarator) break;

            if (!consume(',')) break;
        }

        if (!last_declarator) require(';');

        declaration->text = end_text(begin_text);
    } else {
        ASTNode* statement{};
        if (token == '{') {
            statement = parse_compound_statement();
        } else if (consume(TOK_RETURN)) {
            statement = new ReturnStatement(parse_expr(0), location);
            require(';');
        } else {
            statement = parse_expr(0);
            require(';');
        }

        list.push_back(statement);

        statement->text = end_text(begin_text);
    }
}

CompoundStatement* Parser::parse_compound_statement() {
    CompoundStatement* statement{};
    auto begin_text = data();
    ASTNodeVector list;
    if (preparse) {
        Location loc = lexer.location();
        auto count = 0;
        do {
            if (token == '{') {
                ++count;
            } else if (token == '}') {
                --count;
            }
            consume();
        } while (count != 0);

        statement = new CompoundStatement(move(list), loc);
    } else {
        symbols.push_scope();

        Location loc;
        require('{', &loc);

        while (token && token != '}') {
            parse_declaration_or_statement(IdentifierScope::BLOCK, list);
        }

        // Must pop scope before consuming '}' in case '}' is immediately followed by an identifier that the
        // lexer must correctly identify as TOK_IDENTIFIER or TOK_TYPEDEF_IDENTIFIER.
        symbols.pop_scope();

        require('}');

        statement = new CompoundStatement(move(list), loc);
    }

    statement->text = end_text(begin_text);
    return statement;
}

Declarator* Parser::parse_parameter_declarator() {
    auto location = lexer.location();
    auto begin_declaration_text = data();

    StorageClass storage_class = StorageClass::NONE;
    const Type* type{};
    uint32_t specifiers;
    if (!parse_declaration_specifiers(IdentifierScope::PROTOTYPE, storage_class, type, specifiers)) {
        // TODO
    }

    if (storage_class != StorageClass::NONE && storage_class != StorageClass::REGISTER) {
        message(Severity::ERROR, location) << "invalid storage class\n";
        storage_class = StorageClass::NONE;
    }

    bool last;
    auto declaration = new Declaration(IdentifierScope::PROTOTYPE, storage_class, type, location);

    auto begin_declarator_text = data();
    auto declarator = parse_declarator(declaration, specifiers, false, location, &last);
    declarator->text = end_text(begin_declarator_text);

    declaration->declarators.push_back(declarator);
    declaration->text = end_text(begin_declaration_text);
    return declarator;
}

Declarator* Parser::parse_declarator(Declaration* declaration, uint32_t specifiers, bool allow_function_def, const Location& location, bool* last) {
    auto begin_text = data();
    *last = false;
    if (consume('(')) {
        auto result = parse_declarator(declaration, specifiers, allow_function_def, location, last);
        require(')');
        return result;
    } else {
        const Type* type = declaration->base_type;
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
        if (!consume(TOK_IDENTIFIER)) identifier.name = empty_interned_string;

        Expr* initializer{};
        Declarator* declarator{};

        if (consume('=')) {
            initializer = parse_expr(ASSIGN_PREC);
        } else if (consume('(')) {
            symbols.push_scope();

            vector<Variable*> params;
            vector<const Type*> param_types;
            bool seen_void = false;
            while (token && !consume(')')) {
                auto declarator = parse_parameter_declarator();

                // Functions are adjusted to variable of function pointer type.
                auto variable = dynamic_cast<Variable*>(declarator);
                assert(variable);

                params.push_back(variable);

                if (declarator->type == &VoidType::it) {
                    if (seen_void || !param_types.empty()) {
                        message(Severity::ERROR, declarator->location) << "a parameter may not have void type\n";
                    }
                    seen_void = true;
                } else {
                    param_types.push_back(declarator->type);
                }
                consume(',');
            }

            type = FunctionType::of(type, move(param_types), false);

            if (declaration->scope == IdentifierScope::PROTOTYPE) {
                type = type->pointer_to();
            } else if (declaration->storage_class != StorageClass::TYPEDEF) {
                CompoundStatement* body{};
                if (allow_function_def && token == '{') {
                    body = parse_compound_statement();
                    *last = true;
                }
                    
                declarator = new Function(declaration,
                                          static_cast<const FunctionType*>(type),
                                          specifiers,
                                          identifier,
                                          move(params),
                                          body,
                                          location);
            }

            symbols.pop_scope();
        }

        if (!declarator && (specifiers & (1 << TOK_INLINE))) {
            message(Severity::ERROR, location) << "'inline' may only appear on function\n";
        }

        if (declaration->storage_class == StorageClass::TYPEDEF) {
            declarator = new TypeDef(declaration, type, identifier, location);
        }

        if (!declarator) {
            if (!initializer && declaration->storage_class != StorageClass::EXTERN) {
                initializer = new DefaultExpr(type, location);
            }
            declarator = new Variable(declaration, type, identifier, initializer, location);
        }

        if (!identifier.name->empty()) {
            symbols.add_declarator(TypeNameKind::ORDINARY, declarator);
        }

        declarator->text = end_text(begin_text);
        return declarator;
    }
}
