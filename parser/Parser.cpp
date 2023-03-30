#include "Parser.h"

#include "ASTNode.h"
#include "Constant.h"
#include "Declaration.h"
#include "Expr.h"
#include "lexer/Token.h"
#include "Message.h"
#include "Statement.h"

Parser::Parser(Preprocessor& preprocessor, bool preparse): preprocessor(preprocessor), preparse(preparse), symbols(preparse) {
    token = preprocessor.next_token();
}

void Parser::consume() {
    token = preprocessor.next_token();
}

bool Parser::consume(int t, Location* location) {
    if (token != t) return false;
    if (location) {
        *location = preprocessor.location();
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
            *location = preprocessor.location();
        }
        consume();
        return true;
    }
    return false;
}

void Parser::skip() {
    unexpected_token();
    consume();
}

void Parser::unexpected_token() {
    message(Severity::ERROR, preprocessor.location()) << "unexpected token\n";
}

size_t Parser::position() const {
    return preprocessor.fragment.position;
}

Fragment Parser::end_fragment(size_t begin_position) const {
    return Fragment(begin_position, position() - begin_position);
}

OperatorAssoc Parser::assoc() {
    assert(token >= 0 && token < TOK_NUM);
    return g_assoc_prec[token].assoc;
}

OperatorPrec Parser::prec() {
    assert(token >= 0 && token < TOK_NUM);
    return g_assoc_prec[token].prec;
}

bool Parser::check_eof() {
    if (token == TOK_EOF) return true;
    message(Severity::ERROR, preprocessor.location()) << "expected end of file\n";
    return false;
}

Expr* Parser::parse_expr(int min_prec) {
    Location loc;
    auto begin = position();

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
                loc = preprocessor.location();
                op = BinaryOp(token);
                consume();
                break;
            default:
                return result;
            }

            auto right = parse_expr(next_min_prec);
            result = new BinaryExpr(result, right, op, loc);
        }

        result->fragment = end_fragment(begin);
    }

    return result;
}

Expr* Parser::parse_cast_expr() {
    Expr* result{};
    auto begin = position();

    while (token) {
        if (token == TOK_BIN_INT_LITERAL || token == TOK_OCT_INT_LITERAL || token == TOK_DEC_INT_LITERAL || token == TOK_HEX_INT_LITERAL || token == TOK_CHAR_LITERAL) {
            result = IntegerConstant::of(preprocessor.text(), token, preprocessor.location());
            consume();
            break;
        }
        else if (token == TOK_DEC_FLOAT_LITERAL || token == TOK_HEX_FLOAT_LITERAL) {
            result = FloatingPointConstant::of(preprocessor.text(), token, preprocessor.location());
            consume();
            break;
        }
        else if (token == TOK_STRING_LITERAL) {
            result = StringConstant::of(preprocessor.text(), preprocessor.location());
            consume();
            break;
        }
        else if (token == TOK_IDENTIFIER) {
            Declarator* declarator = symbols.lookup_declarator(preprocessor.identifier());
            if (declarator) {
                // TokenKind would have to be TOK_TYPEDEF_IDENTIFIER for declarator to be a typedef.
                assert(!dynamic_cast<TypeDef*>(declarator));

                result = new NameExpr(declarator, preprocessor.location());
            } else {
                message(Severity::ERROR, preprocessor.location()) << '\'' << preprocessor.identifier() << "' undeclared\n";
                result = IntegerConstant::default_expr(preprocessor.location());
            }
            consume();
            break;
        } else if (consume('(')) {
            result = parse_expr(0);
            require(')');
        }
        else {
            skip();
            begin = position();
        }
    }

    if (!result) {
        assert(token == TOK_EOF);
        message(Severity::ERROR, preprocessor.location()) << "unexpected end of file\n";
        result = IntegerConstant::default_expr(preprocessor.location());
    }

    result->fragment = end_fragment(begin);
    return result;
}

bool Parser::parse_declaration_specifiers(IdentifierScope scope, StorageClass& storage_class, const Type*& type, uint32_t& specifiers) {
    const uint32_t storage_class_mask = (1 << TOK_TYPEDEF) | (1 << TOK_EXTERN) | (1 << TOK_STATIC) | (1 << TOK_AUTO) | (1 << TOK_REGISTER);
    const uint32_t type_qualifier_mask = (1 << TOK_CONST) | (1 << TOK_RESTRICT) | (1 << TOK_VOLATILE);
    const uint32_t function_specifier_mask = 1 << TOK_INLINE;
    const uint32_t type_specifier_mask = ~(storage_class_mask | type_qualifier_mask | function_specifier_mask);

    Location storage_class_location;
    Location type_specifier_location = preprocessor.location();
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
                message(Severity::ERROR, preprocessor.location()) << "invalid type specifier combination\n";
            }
            specifier_set &= ~(1 << token);
        }

        Location specifier_location = preprocessor.location();
        TokenKind found_specifier{};
        switch (token) {
            case TOK_TYPEDEF:
            case TOK_EXTERN:
            case TOK_STATIC:
            case TOK_AUTO:
            case TOK_REGISTER: {
              found_specifier = token;
              storage_class_location = preprocessor.location();
              consume();
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
              found_specifier = token;
              type_specifier_location = preprocessor.location();                
              consume();
              break;

          } case TOK_IDENTIFIER: {
              if ((specifier_set & type_specifier_mask) == 0) {
                  const Type* typedef_type;
                  typedef_type = symbols.lookup_type(preprocessor.identifier());
                  if (!typedef_type) {
                      if (scope != IdentifierScope::FILE) break;

                      if (preparse) {
                          typedef_type = NamedType::of(TypeNameKind::ORDINARY, preprocessor.identifier());
                      } else {
                          message(Severity::ERROR, preprocessor.location()) << "typedef \'" << preprocessor.identifier() << "' undefined\n";
                          typedef_type = IntegerType::default_type();
                      }
                  }

                  type = typedef_type;
                  found_specifier = token;
                  type_specifier_location = preprocessor.location();                
                  consume();
              }
              break;

          } case TOK_STRUCT:
            case TOK_UNION: {
              found_specifier = token;
              type_specifier_location = preprocessor.location();
              consume();

              Identifier identifier;
              if (token == TOK_IDENTIFIER) {
                  identifier = preprocessor.identifier();
                  consume();
              }

              if (consume('{')) {
                  vector<Declaration*> members;
                  while (token && token != '}') {
                      auto node = parse_declaration_or_statement(IdentifierScope::FILE);
                      members.push_back(dynamic_cast<Declaration*>(node));
                  }

                  require('}');

                  if (found_specifier == TOK_STRUCT) {
                      type = new StructType(move(members), type_specifier_location);
                  } else {
                      type = new UnionType(move(members), type_specifier_location);
                  }
              }

              if (identifier.name->empty()) {
                  if (!type) {
                      unexpected_token();
                  }
              } else {
                  if (type) {
                      auto declarator = new TypeDef(nullptr, type, identifier, type_specifier_location);
                      type = new DeclarationType(declarator);
                      symbols.add_declarator(declarator);
                  } else {
                      if (preparse) {
                          if (found_specifier == TOK_STRUCT) {
                              type = NamedType::of(TypeNameKind::STRUCT, identifier);
                          } else {
                              type = NamedType::of(TypeNameKind::UNION, identifier);
                          }
                      } else {
                          // TODO
                      }
                  }
              }
              
              break;

          } case TOK_INLINE: {
              found_specifier = token;
              function_specifier_location = preprocessor.location();
              specifier_set &= ~(1 << token); // function speficiers may be repeated
              consume();
              break;

          } case TOK_CONST:
            case TOK_RESTRICT:
            case TOK_VOLATILE: {
              found_specifier = token;
              qualifier_location = preprocessor.location();
              specifier_set &= ~(1 << token); // qualifiers may be repeated
              consume();
              break;
          }
        }

        if (found_specifier) {
            assert(found_specifier < 32);
            is_declaration = true;
            if (specifier_set & (1 << found_specifier)) {
                message(Severity::ERROR, specifier_location) << "invalid declaration specifier or type qualifier combination\n";
            }
            specifier_set |= 1 << found_specifier;
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

      } case (1 << TOK_IDENTIFIER):
        case (1 << TOK_STRUCT):
        case (1 << TOK_UNION): {
          break;
      }
    }

    if (specifier_set & type_qualifier_mask) {
        type = QualifiedType::of(type, specifier_set & type_qualifier_mask);
    }

    specifiers = specifier_set & function_specifier_mask;
    return true;
}

void Parser::parse_unit() {
    while (token) {
        declarations.push_back(parse_declaration_or_statement(IdentifierScope::FILE));
    }
}

ASTNode* Parser::parse_declaration_or_statement(IdentifierScope scope) {
    auto location = preprocessor.location();
    auto begin = position();

    StorageClass storage_class = StorageClass::NONE;
    const Type* type{};
    uint32_t specifiers;
    auto mark_root = preprocessor.mark_root();
    if (parse_declaration_specifiers(scope, storage_class, type, specifiers)) {
        auto declaration = new Declaration(scope, storage_class, type, location);
        declaration->mark_root = mark_root;

        if (auto declaration_type = dynamic_cast<const DeclarationType*>(type)) {
            auto declarator = const_cast<TypeDef*>(declaration_type->declarator);
            declarator->declaration = declaration;
        }

        int declarator_count = 0;
        bool last_declarator{};
        while (token && token != ';') {
            auto declarator = parse_declarator(declaration, specifiers, declarator_count == 0, location, &last_declarator);

            if (declarator->identifier.name->empty()) {
                message(Severity::ERROR, preprocessor.location()) << "expected identifier\n";
            } else {
                declaration->declarators.push_back(declarator);
                ++declarator_count;
            }

            // No ';' or ',' after function definition.
            if (last_declarator) break;

            if (!consume(',')) break;
        }

        if (!last_declarator) require(';');

        declaration->fragment = end_fragment(begin);
        return declaration;
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

        statement->fragment = end_fragment(begin);
        return statement;
    }
}

CompoundStatement* Parser::parse_compound_statement() {
    CompoundStatement* statement{};
    auto begin = position();
    ASTNodeVector list;
    if (preparse) {
        Location loc = preprocessor.location();
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
            list.push_back(parse_declaration_or_statement(IdentifierScope::BLOCK));
        }

        // Must pop scope before consuming '}' in case '}' is immediately followed by an identifier that the
        // preprocessor must correctly identify as TOK_IDENTIFIER or TOK_TYPEDEF_IDENTIFIER.
        symbols.pop_scope();

        require('}');

        statement = new CompoundStatement(move(list), loc);
    }

    statement->fragment = end_fragment(begin);
    return statement;
}

Declarator* Parser::parse_parameter_declarator() {
    auto location = preprocessor.location();
    auto begin_declaration = position();

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

    auto begin_declarator = position();
    auto declarator = parse_declarator(declaration, specifiers, false, location, &last);
    declarator->fragment = end_fragment(begin_declarator);

    declaration->declarators.push_back(declarator);
    declaration->fragment = end_fragment(begin_declaration);
    return declarator;
}

Declarator* Parser::parse_declarator(Declaration* declaration, uint32_t specifiers, bool allow_function_def, const Location& location, bool* last) {
    auto begin = position();
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

        auto identifier = preprocessor.identifier();
        if (!consume(TOK_IDENTIFIER)) identifier.name = empty_interned_string;

        Expr* initializer{};
        Declarator* declarator{};

        while (consume('[')) {
            Expr* array_size{};
            if (token != ']') {
                array_size = parse_expr(ASSIGN_PREC);
            }
            require(']');

            type = new ArrayType(type, array_size);
        }

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
            symbols.add_declarator(declarator);
        }

        declarator->fragment = end_fragment(begin);
        return declarator;
    }
}
