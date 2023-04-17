#include "Parser.h"

#include "ArrayType.h"
#include "ASTNode.h"
#include "Constant.h"
#include "Declaration.h"
#include "Expr.h"
#include "lexer/Token.h"
#include "Message.h"
#include "Statement.h"
#include "visitor/ResolvePass.h"

enum {
    PD_ALLOW_FUNCTION_DEFINITION  = 0x0001,
    PD_ALLOW_IDENTIFIER           = 0x0002,
    PD_ALLOW_INITIALIZER          = 0x0004,
};

const Type* DeclaratorTransform::apply(const Type* type) {
    if (type_transform) type = type_transform(type);
    return type;
}

Parser::Parser(Preprocessor& preprocessor, IdentifierMap& identifiers): preprocessor(preprocessor), identifiers(identifiers), preparse(preprocessor.preparse) {
    assert(identifiers.preparse == preprocessor.preparse);
    consume();
}

Expr* Parser::parse_standalone_expr() {
    return parse_expr(SEQUENCE_PREC);
}

void Parser::consume() {
    while (token) {
        token = preprocessor.next_token();

        switch (token) {
          default:
            return;
          case TOK_PP_EXTERN:
          case TOK_PP_STATIC:
          case TOK_PP_TYPE:
          case TOK_PP_ENUM:
            handle_declaration_directive();
            break;
        }
    }
}

void Parser::handle_declaration_directive() {
    StorageClass storage_class;
    switch (token) {
      default:
        assert(false);
        break;
      case TOK_PP_ENUM:
      case TOK_PP_TYPE:
        storage_class = StorageClass::NONE;
        break;
      case TOK_PP_STATIC:
        storage_class = StorageClass::STATIC;
        break;
      case TOK_PP_EXTERN:
        storage_class = StorageClass::EXTERN;
        break;
    }

    auto pp_token = preprocessor.next_pp_token();

    auto declaration = new Declaration(IdentifierScope::FILE, storage_class, &UniversalType::it, preprocessor.location());
    declarations.push_back(declaration);

    while (pp_token && pp_token != '\n') {
        if (pp_token == TOK_IDENTIFIER) {
            auto id = preprocessor.identifier();

            auto new_declarator = new Declarator(declaration, id, preprocessor.location());
            switch (token) {
              case TOK_PP_ENUM:
                new_declarator->delegate = new EnumConstant(new_declarator);
                new_declarator->type = IntegerType::default_type();
                break;
              case TOK_PP_STATIC:
              case TOK_PP_EXTERN:
                new_declarator->delegate = new Entity(new_declarator);
                new_declarator->type = &UniversalType::it;
                break;
              case TOK_PP_TYPE:
                new_declarator->delegate = new TypeDef(new_declarator);
                new_declarator->type = new_declarator->to_type();
                break;
              default:
                preprocessor.unexpected_directive_token();
                break;
            }

            if (new_declarator->delegate) {
                auto old_declarator = identifiers.lookup_declarator(id);
                if (old_declarator &&
                    old_declarator->delegate->linkage() == new_declarator->delegate->linkage()) {
                    if (old_declarator->delegate->kind() < new_declarator->delegate->kind()) {
                        *old_declarator = move(*new_declarator);
                        old_declarator->delegate->declarator = old_declarator;
                    }
                } else {
                    identifiers.add_declarator(new_declarator);
                    declaration->declarators.push_back(new_declarator);
                }
            }
        } else {
            preprocessor.unexpected_directive_token();
        }
        pp_token = preprocessor.next_pp_token();
    }
}

bool Parser::consume(int t, Location* location) {
    if (token != t) return false;
    if (location) {
        *location = preprocessor.location();
    }
    consume();
    return true;
}

bool Parser::consume_identifier(Identifier& identifier) {
    if (token != TOK_IDENTIFIER) {
        identifier = Identifier();
        return false;
    }

    identifier = preprocessor.identifier();
    consume();
    return true;
}

void Parser::balance_until(int t) {
    while (token && token != t) {
        switch (token) {
          default:
            consume();
            break;
          case '(':
            consume();
            balance_until(')');
            consume();
            break;
          case '{':
            consume();
            balance_until('}');
            consume();
            break;
          case '[':
            consume();
            balance_until(']');
            consume();
            break;
        }
    }
}

bool Parser::require(int t, Location* location) {
    if (token != t) {
        if (t >= 32 && t < 128) {
            message(Severity::ERROR, preprocessor.location()) << "expected '" << (char) t << "' but got '" << preprocessor.text() << "'\n";
            pause_messages();
        } else {
            skip_unexpected();
        }
    }

    while (token && token != t) {
        consume();
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

void Parser::skip_unexpected() {
    unexpected_token();
    consume();
}

void Parser::unexpected_token() {
    if (token == TOK_EOF) {
        message(Severity::ERROR, preprocessor.location()) << "unexpected end of file\n";
    } else {
        message(Severity::ERROR, preprocessor.location()) << "unexpected token '" << preprocessor.text() << "'\n";
    }
    pause_messages();
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

Expr* Parser::parse_expr(OperatorPrec min_prec) {
    Location loc = preprocessor.location();
    Expr* result{};

    if (preparse) {
        // To make it easier to write tests that run in both preparse and not, decimal integers can always be parsed.
        if (token == TOK_DEC_INT_LITERAL) {
            result = IntegerConstant::of(preprocessor.text(), token, preprocessor.location());
        } else {
            result = IntegerConstant::default_expr(loc);
        }

        skip_expr(min_prec);
        return result;
    }

    result = parse_cast_expr();

    while (prec() >= min_prec) {
        auto next_min_prec = assoc() == ASSOC_LEFT ? OperatorPrec(prec() + 1) : prec();

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
              default:
                return result;
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
            }

            auto right = parse_expr(next_min_prec);
            result = new BinaryExpr(result, right, op, loc);
        }
    }

    return result;
}

void Parser::skip_expr(OperatorPrec min_prec) {
    // The precedence returned by prec() is wrt the use of the token as closing parenthesis,
    // as a binary operator or as the '?' in the conditional operator. The token might instead
    // be, e.g., a unary operator. The highest precedence token for which there is ambiguity
    // is '&'. Therefore we require that min_prec be such that a misinterpreted '&' would not
    // cause skip_expr to return early.
    assert(min_prec < AND_PREC);

    while (token) {
        if (prec() != 0 && prec() <= min_prec) return;

        switch (token) {
          default:
            consume();
            break;
          case '(':
            consume();
            balance_until(')');
            consume();
            break;
          case '{':
            consume();
            balance_until('}');
            consume();
            break;
          case '[':
            consume();
            balance_until(']');
            consume();
            break;
        }
    }
}

Expr* Parser::parse_cast_expr() {
    return parse_unary_expr();
}

Expr* Parser::parse_unary_expr() {
    Location loc;
    Expr* result{};

    while (token && token != '}' && token != ')' && token != ']' && token != ';') {
        if (consume(TOK_SIZEOF, &loc)) {
            auto consumed_paren = consume('(');
            
            const Type* type{};
            if (consumed_paren) type = parse_type_name();

            if (type) {
                result = new SizeOfExpr(type, loc);
            } else {
                auto expr = parse_unary_expr();
                result = new SizeOfExpr(new TypeOfType(expr, loc), loc);
            }

            if (consumed_paren) require(')');

            break;
        } else if (token == TOK_BIN_INT_LITERAL || token == TOK_OCT_INT_LITERAL || token == TOK_DEC_INT_LITERAL || token == TOK_HEX_INT_LITERAL || token == TOK_CHAR_LITERAL) {
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
            Declarator* declarator = identifiers.lookup_declarator(preprocessor.identifier());
            if (declarator) {
                // TokenKind would have to be TOK_TYPEDEF_IDENTIFIER for declarator to be a typedef.
                assert(!declarator->type_def());

                result = new EntityExpr(declarator, preprocessor.location());
            } else {
                message(Severity::ERROR, preprocessor.location()) << '\'' << preprocessor.identifier() << "' undeclared\n";
                result = IntegerConstant::default_expr(preprocessor.location());
            }
            consume();
            break;
        } else if (consume('(')) {
            result = parse_expr(SEQUENCE_PREC);
            require(')');
        } else {
            skip_unexpected();
        }
    }

    if (!result) {
        unexpected_token();
        result = IntegerConstant::default_expr(preprocessor.location());
    }

    return result;
}

Expr* Parser::parse_initializer() {
    if (consume('{')) {
        auto initializer = new InitializerExpr(preprocessor.location());

        while (token && token != '}') {
            initializer->elements.push_back(parse_initializer());
            if (token != '}') require(',');
        }

        require('}');
        return initializer;
    } else {
        return parse_expr(ASSIGN_PREC);
    }
}

Declaration* Parser::parse_declaration_specifiers(IdentifierScope scope, const Type*& type, uint32_t& specifiers) {
    const uint32_t storage_class_mask = (1 << TOK_TYPEDEF) | (1 << TOK_EXTERN) | (1 << TOK_STATIC) | (1 << TOK_AUTO) | (1 << TOK_REGISTER);
    const uint32_t type_qualifier_mask = (1 << TOK_CONST) | (1 << TOK_RESTRICT) | (1 << TOK_VOLATILE);
    const uint32_t function_specifier_mask = 1 << TOK_INLINE;
    const uint32_t type_specifier_mask = ~(storage_class_mask | type_qualifier_mask | function_specifier_mask);

    Declaration* declaration{};
    Location declaration_location = preprocessor.location();
    StorageClass storage_class;
    Location storage_class_location;
    Location type_specifier_location = preprocessor.location();
    Location function_specifier_location;
    Location qualifier_location;
    uint32_t specifier_set = 0;
    uint32_t qualifier_set = 0;
    int num_longs = 0;
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
                  const Type* typedef_type{};
                  if (preparse) {
                      typedef_type = UnboundType::of(preprocessor.identifier());
                  } else {
                      auto declarator = identifiers.lookup_declarator(preprocessor.identifier());
                      if (declarator) {
                          typedef_type = declarator->to_type();
                      }

                      if (!typedef_type) {
                          // No error in scopes where something other than a type would be valid, i.e. a statement or expression.
                          if (scope == IdentifierScope::BLOCK || scope == IdentifierScope::EXPRESSION) break;

                          message(Severity::ERROR, preprocessor.location()) << "type \'" << preprocessor.identifier() << "' undefined\n";
                          typedef_type = IntegerType::default_type();
                      }
                  }

                  type = typedef_type;
                  found_specifier = token;
                  type_specifier_location = preprocessor.location();                
                  consume();
              }
              break;

          } case TOK_ENUM:
            case TOK_STRUCT:
            case TOK_UNION: {
              found_specifier = token;
              type_specifier_location = preprocessor.location();
              if (!declaration) declaration = new Declaration(scope, declaration_location);
              type = parse_structured_type(declaration);
              break;

          } case TOK_TYPEOF:
            case TOK_TYPEOF_UNQUAL: {
              found_specifier = token;
              type_specifier_location = preprocessor.location();
              type = parse_typeof();
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
            if (!declaration) declaration = new Declaration(scope, declaration_location);
            if (specifier_set & (1 << found_specifier)) {
                message(Severity::ERROR, specifier_location) << "invalid declaration specifier or type qualifier combination\n";
            }
            specifier_set |= 1 << found_specifier;
        } else {
            break;
        }
    }

    if (!declaration) return declaration;

    // Have single storage class specifier iff storage class set contains only one element.
    uint32_t storage_class_set = specifier_set & storage_class_mask;
    if (storage_class_set != 0 && storage_class_set & (storage_class_set-1)) {
        message(Severity::ERROR, storage_class_location) << "too many storage classes\n";
    }

    storage_class = StorageClass::NONE;
    if (storage_class_set & (1 << TOK_STATIC))        storage_class = StorageClass::STATIC;
    else if (storage_class_set & (1 << TOK_EXTERN))   storage_class = StorageClass::EXTERN;
    else if (storage_class_set & (1 << TOK_TYPEDEF))  storage_class = StorageClass::TYPEDEF;
    else if (storage_class_set & (1 << TOK_AUTO))     storage_class = StorageClass::AUTO;
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

      } case (1 << TOK_ENUM):
        case (1 << TOK_IDENTIFIER):
        case (1 << TOK_STRUCT):
        case (1 << TOK_UNION):
        case (1 << TOK_TYPEOF):
        case (1 << TOK_TYPEOF_UNQUAL): {
          break;
      }
    }

    if (specifier_set & type_qualifier_mask) {
        type = QualifiedType::of(type, specifier_set & type_qualifier_mask);
    }

    declaration->storage_class = storage_class;
    declaration->type = type;

    specifiers = specifier_set & function_specifier_mask;
    return declaration;
}

void Parser::parse() {
    while (token) {
        declarations.push_back(parse_declaration_or_statement(IdentifierScope::FILE));
    }

    if (!preparse) {
        resolve_pass(identifiers.scopes.front());
    }
}

ASTNode* Parser::parse_declaration_or_statement(IdentifierScope scope) {
    resume_messages();

    auto location = preprocessor.location();
    auto begin = position();

    const Type* base_type{};
    uint32_t specifiers;
    auto mark_root = preprocessor.mark_root();
    if (auto declaration = parse_declaration_specifiers(scope, base_type, specifiers)) {
        declaration->mark_root = mark_root;

        int declarator_count = 0;
        bool last_declarator{};
        while (token && token != ';') {
            int flags = PD_ALLOW_IDENTIFIER | PD_ALLOW_INITIALIZER;
            if (declarator_count == 0) flags |= PD_ALLOW_FUNCTION_DEFINITION;
            auto declarator = parse_declarator(declaration, base_type, specifiers, flags, &last_declarator);
            if (declarator->identifier.name->empty()) {
                message(Severity::ERROR, preprocessor.location()) << "expected identifier but got '" << preprocessor.text() << "'\n";
                pause_messages();
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
            statement = new ReturnStatement(parse_expr(SEQUENCE_PREC), location);
            require(';');
        } else {
            statement = parse_expr(SEQUENCE_PREC);
            require(';');
        }

        return statement;
    }
}

CompoundStatement* Parser::parse_compound_statement() {
    CompoundStatement* statement{};
    ASTNodeVector list;
    if (preparse) {
        Location loc;
        require('{', &loc);
        balance_until('}');
        require('}');


        statement = new CompoundStatement(move(list), loc);
    } else {
        identifiers.push_scope();

        Location loc;
        require('{', &loc);

        while (token && token != '}') {
            list.push_back(parse_declaration_or_statement(IdentifierScope::BLOCK));
        }

        // Must pop scope before consuming '}' in case '}' is immediately followed by an identifier that the
        // preprocessor must correctly identify as TOK_IDENTIFIER or TOK_TYPEDEF_IDENTIFIER.
        identifiers.pop_scope();

        require('}');

        statement = new CompoundStatement(move(list), loc);
    }

    return statement;
}

Declarator* Parser::parse_parameter_declarator() {
    auto begin_declaration = position();

    const Type* base_type;
    uint32_t specifiers;
    auto declaration = parse_declaration_specifiers(IdentifierScope::PROTOTYPE, base_type, specifiers);
    if (!declaration) {
        // TODO
    }

    if (declaration->storage_class != StorageClass::NONE && declaration->storage_class != StorageClass::REGISTER) {
        message(Severity::ERROR, declaration->location) << "invalid storage class\n";
        declaration->storage_class = StorageClass::NONE;
    }

    bool last;
    auto begin_declarator = position();
    auto declarator = parse_declarator(declaration, base_type, specifiers, PD_ALLOW_IDENTIFIER, &last);
    declarator->fragment = end_fragment(begin_declarator);

    declaration->declarators.push_back(declarator);
    declaration->fragment = end_fragment(begin_declaration);
    return declarator;
}

DeclaratorTransform Parser::parse_declarator_transform(IdentifierScope scope, int flags) {
    function<const Type*(const Type*)> left_transform;
    while (consume('*')) {
        left_transform = [left_transform](const Type* type) {
            if (left_transform) type = left_transform(type);
            return type->pointer_to();
        };

        unsigned qualifier_set = 0;
        while (token == TOK_CONST || token == TOK_RESTRICT || token == TOK_VOLATILE) {
            qualifier_set |= 1 << token;
            consume();
        }

        if (qualifier_set) {
            left_transform = [left_transform, qualifier_set](const Type* type) {
                if (left_transform) type = left_transform(type);
                return QualifiedType::of(type, qualifier_set);
            };
        }
    }

    DeclaratorTransform declarator;
    if (consume('(')) {
        declarator = parse_declarator_transform(scope, flags);
        require(')');
    } else {
        if (flags & PD_ALLOW_IDENTIFIER) consume_identifier(declarator.identifier);
    }

    auto location = preprocessor.location();
    function<const Type*(const Type*)> right_transform;
    for (int depth = 0; token; ++depth) {
        if (consume('[')) {
            // C99 6.7.5.3p7
            unsigned array_qualifier_set{};
            if (depth == 0 && scope == IdentifierScope::PROTOTYPE) {
                while (token == TOK_CONST || token == TOK_RESTRICT || token == TOK_VOLATILE || token == TOK_STATIC) {
                    if (token != TOK_STATIC) {
                        array_qualifier_set |= 1 << token;
                    }
                    consume();
                }
            }

            Expr* array_size{};
            if (token != ']') {
                array_size = parse_expr(ASSIGN_PREC);
            }
            require(']');

            right_transform = [right_transform, array_qualifier_set, array_size, location](const Type* type) {
                type = QualifiedType::of(new UnresolvedArrayType(type, array_size, location), array_qualifier_set);
                if (right_transform) type = right_transform(type);
                return type;
            };

        } else if (consume('(')) {
            identifiers.push_scope();

            vector<const Type*> param_types;
            bool seen_void = false;
            while (token && !consume(')')) {
                auto param_declarator = parse_parameter_declarator();

                // Functions are adjusted to variable of function pointer type.
                auto entity = param_declarator->entity();
                assert(entity);

                declarator.params.push_back(entity);

                if (param_declarator->type == &VoidType::it) {
                    if (seen_void || !param_types.empty()) {
                        message(Severity::ERROR, param_declarator->location) << "a parameter may not have void type\n";
                    }
                    seen_void = true;
                } else {
                    param_types.push_back(param_declarator->type);
                }
                consume(',');
            }

            right_transform = [right_transform, param_types=move(param_types)](const Type* type) {
                type = FunctionType::of(type, move(param_types), false);
                if (right_transform) type = right_transform(type);
                return type;
            };

            if ((flags & PD_ALLOW_FUNCTION_DEFINITION) && token == '{') {
                declarator.body = parse_compound_statement();
            }

            identifiers.pop_scope();
        } else {
            break;
        }

        location = preprocessor.location();
    }

    if (left_transform || right_transform) {
        declarator.type_transform = [left_transform, right_transform, inner_transform = declarator.type_transform](const Type* type) {
            if (left_transform) type = left_transform(type);
            if (right_transform) type = right_transform(type);
            if (inner_transform) type = inner_transform(type);
            return type;
        };
    }

    return declarator;
}

Declarator* Parser::parse_declarator(Declaration* declaration, const Type* type, uint32_t specifiers, int flags, bool* last) {
    auto location = preprocessor.location();
    auto begin = position();

    auto declarator_transform = parse_declarator_transform(declaration->scope, flags);
    *last = declarator_transform.body;
    if (declarator_transform.identifier.name->empty()) location = declaration->location;

    type = declarator_transform.apply(type);

    bool is_function = declaration->scope != IdentifierScope::PROTOTYPE && dynamic_cast<const FunctionType*>(type);

    Expr* bit_field_size{};
    if (declaration->scope == IdentifierScope::STRUCTURED && consume(':')) {
        bit_field_size = parse_expr(CONDITIONAL_PREC);
    }

    Expr* initializer{};
    if ((flags & PD_ALLOW_INITIALIZER) && consume('=')) {
        initializer = parse_initializer();
    }

    auto declarator = new Declarator(declaration, type, declarator_transform.identifier, location);

    if (is_function && declaration->storage_class != StorageClass::TYPEDEF) {
        declarator->delegate = new Entity(declarator,
                                          specifiers,
                                          move(declarator_transform.params),
                                          declarator_transform.body);
    } else {
        if (specifiers & (1 << TOK_INLINE)) {
            message(Severity::ERROR, location) << "'inline' may only appear on function\n";
        }

        if (declaration->storage_class == StorageClass::TYPEDEF) {
            declarator->delegate = new TypeDef(declarator);
        } else {
            declarator->delegate = new Entity(declarator, initializer, bit_field_size);
        }
    }

    if (!declarator->identifier.name->empty()) {
        identifiers.add_declarator(declarator);
    }

    declarator->fragment = end_fragment(begin);
    return declarator;
}

Declarator* Parser::declare_tag_type(Declaration* declaration, const Identifier& identifier, TagType* type, const Location& location) {
    auto declarator = new Declarator(declaration, identifier, location);
    declarator->type = type;
    declarator->delegate = new TypeDef(declarator);
    type->tag = declarator;
    identifiers.add_declarator(declarator);
    return declarator;
}

const Type* Parser::parse_structured_type(Declaration* declaration) {
    auto specifier = token;
    auto specifier_location = preprocessor.location();
    consume();

    Identifier identifier;
    consume_identifier(identifier);

    // C99 6.7.2.3p9
    Declarator* tag_declarator{};
    if (token != ';' && token != '{' && !identifier.name->empty()) {
        tag_declarator = identifiers.lookup_declarator(identifier);
        if (tag_declarator) {
            return tag_declarator->type;
        }
    }

    bool anonymous = declaration->scope == IdentifierScope::STRUCTURED && identifier.name->empty();

    TagType* type{};

    if (specifier != TOK_ENUM) {
        StructuredType* structured_type;
        if (specifier == TOK_STRUCT) {
            structured_type = new StructType(specifier_location);
        } else {
            structured_type = new UnionType(specifier_location);
        }
        type = structured_type;

        if (consume('{')) {
            // C99 6.7.2.3p6
            if (!identifier.name->empty()) tag_declarator = declare_tag_type(declaration, identifier, type, specifier_location);

            structured_type->complete = true;

            if (!anonymous) identifiers.push_scope();

            while (token && token != '}') {
                auto member_declaration = dynamic_cast<Declaration*>(parse_declaration_or_statement(IdentifierScope::STRUCTURED));
                if (!member_declaration) continue;

                // C11 6.7.2.1p13 anonymous structs and unions
                if (dynamic_cast<const StructuredType*>(member_declaration->type) && member_declaration->declarators.empty()) {
                    auto member_declarator = new Declarator(member_declaration, member_declaration->type, Identifier(), member_declaration->location);
                    member_declarator->delegate = new Entity(member_declarator);
                    member_declaration->declarators.push_back(member_declarator);
                }

                for (auto member: member_declaration->declarators) {
                    structured_type->members.push_back(member);
                }
            }

            if (!anonymous) {
                structured_type->member_index = identifiers.pop_scope().declarators;
            }

            require('}');
        }
    } else {
        auto enum_type = new EnumType(specifier_location);
        type = enum_type;

        if (consume('{')) {
            // C99 6.7.2.3p6
            if (!identifier.name->empty()) tag_declarator = declare_tag_type(declaration, identifier, type, specifier_location);

            enum_type->complete = true;
            while (token && token != '}') {
                auto declarator = parse_enum_constant(declaration, enum_type, tag_declarator);
                if (declarator) {
                    enum_type->constants.push_back(declarator);
                }
            }

            require('}');
        }
    }

    if (!tag_declarator && !identifier.name->empty()) {
        if (token == ';') {
            // C99 6.7.2.3p7
            tag_declarator = declare_tag_type(declaration, identifier, type, specifier_location);
        } else {
            // C99 6.7.2.3p8
            declaration = new Declaration(IdentifierScope::FILE, specifier_location);
            tag_declarator = declare_tag_type(declaration, identifier, type, specifier_location);
        }
    }

    return type;
}

Declarator* Parser::parse_enum_constant(Declaration* declaration, const EnumType* type, Declarator* tag) {
    auto location = preprocessor.location();

    Identifier identifier;
    if (!consume_identifier(identifier)) {
        require(TOK_IDENTIFIER);
    }

    Expr* constant{};
    if (consume('=')) {
        constant = parse_expr(CONDITIONAL_PREC);
    }

    if (!consume(',')) {
        if (token != '}') {
            unexpected_token();
        }
    }

    auto declarator = new Declarator(declaration, IntegerType::default_type(), identifier, location);
    auto enum_constant = new EnumConstant(declarator, tag, constant);
    declarator->delegate = enum_constant;
    identifiers.add_declarator(declarator);
    declarator = identifiers.lookup_declarator(identifier);
    enum_constant = declarator->enum_constant();

    return declarator;
}

const Type* Parser::parse_typeof() {
    auto location = preprocessor.location();

    bool keep_qualifiers = token == TOK_TYPEOF;
    consume();
    require('(');

    auto type = parse_type_name();
    if (!type) {
        auto expr = parse_expr(SEQUENCE_PREC);
        type = new TypeOfType(expr, location);
    }

    if (!keep_qualifiers) type = new UnqualifiedType(type);

    require(')');
    return type;
}

const Type* Parser::parse_type_name() {
    const Type* type{};
    uint32_t specifiers{};
    auto declaration = parse_declaration_specifiers(IdentifierScope::EXPRESSION, type, specifiers);
    if (!declaration) return nullptr;

    if (token && token != ')') {
        auto transform = parse_declarator_transform(IdentifierScope::EXPRESSION, 0);
        type = transform.apply(type);
    }

    return type;
}
