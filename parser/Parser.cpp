#include "Parser.h"

#include "ASTNode.h"
#include "Constant.h"
#include "Declaration.h"
#include "Expr.h"
#include "lexer/Token.h"
#include "Message.h"
#include "Statement.h"

Parser::Parser(Preprocessor& preprocessor, SymbolMap& symbols): preprocessor(preprocessor), symbols(symbols), preparse(preprocessor.preparse) {
    consume();
}

void Parser::consume() {
    while (token) {
        token = preprocessor.next_token();

        switch (token) {
          default:
            return;
          case TOK_PP_TYPE:
            handle_type_directive();
            break;
        }
    }
}

void Parser::handle_type_directive() {
    auto pp_token = preprocessor.next_pp_token();

    while (pp_token && pp_token != '\n') {
        if (pp_token == TOK_IDENTIFIER) {
            // This defines a type recursively as itself, which will be compatible with any existing or future
            // definitiom, i.e. same as typedef T T;
            auto id = preprocessor.identifier();
            auto type = NamedType::of(TOK_IDENTIFIER, id);
            auto declaration = new Declaration(IdentifierScope::FILE, StorageClass::TYPEDEF, preprocessor.location());
            auto declarator = new TypeDef(declaration, type, id, preprocessor.location());
            symbols.add_declarator(declarator);
        } else {
            unexpected_token();
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

void Parser::balance_until(int t) {
    while (token && token != t) {
        switch (token) {
          default:
            consume();
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
    while (token && token != t) {
        skip_unexpected();
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

Expr* Parser::parse_expr(OperatorPrec min_prec) {
    Location loc = preprocessor.location();
    auto begin = position();

    // To make it easier to write tests that run in both preparse and not, decimal integers can always be parsed.
    if (preparse && token != TOK_DEC_INT_LITERAL) {
        skip_expr(min_prec);
        return IntegerConstant::default_expr(loc);
    }

    auto result = parse_cast_expr();

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

        result->fragment = end_fragment(begin);
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
            result = parse_expr(SEQUENCE_PREC);
            require(')');
        }
        else {
            skip_unexpected();
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
                  const Type* typedef_type;
                  typedef_type = symbols.lookup_type(preprocessor.identifier());
                  if (!typedef_type) {
                      if (scope == IdentifierScope::BLOCK) break;

                      if (preparse) {
                          typedef_type = NamedType::of(TOK_IDENTIFIER, preprocessor.identifier());
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

          } case TOK_ENUM:
            case TOK_STRUCT:
            case TOK_UNION: {
              found_specifier = token;
              type_specifier_location = preprocessor.location();
              if (!declaration) declaration = new Declaration(scope, declaration_location);
              type = parse_structured_type(declaration);
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
        case (1 << TOK_UNION): {
          break;
      }
    }

    if (specifier_set & type_qualifier_mask) {
        type = QualifiedType::of(type, specifier_set & type_qualifier_mask);
    }

    declaration->initialize(storage_class);

    specifiers = specifier_set & function_specifier_mask;
    return declaration;
}

ASTNodeVector Parser::parse_unit() {
    symbols.clear_internal_linkage();

    ASTNodeVector declarations;
    while (token) {
        declarations.push_back(parse_declaration_or_statement(IdentifierScope::FILE));
    }
    return declarations;
}

ASTNode* Parser::parse_declaration_or_statement(IdentifierScope scope) {
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
            auto declarator = parse_declarator(declaration, base_type, specifiers, declarator_count == 0, location, &last_declarator);
            if (declarator) {
                if (declarator->identifier.name->empty()) {
                    message(Severity::ERROR, preprocessor.location()) << "expected identifier\n";
                } else {
                    declaration->declarators.push_back(declarator);
                    ++declarator_count;
                }
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

        statement->fragment = end_fragment(begin);
        return statement;
    }
}

CompoundStatement* Parser::parse_compound_statement() {
    CompoundStatement* statement{};
    auto begin = position();
    ASTNodeVector list;
    if (preparse) {
        Location loc;
        require('{', &loc);
        balance_until('}');
        require('}');


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
    auto declarator = parse_declarator(declaration, base_type, specifiers, false, declaration->location, &last);
    declarator->fragment = end_fragment(begin_declarator);

    declaration->declarators.push_back(declarator);
    declaration->fragment = end_fragment(begin_declaration);
    return declarator;
}

Declarator* Parser::parse_declarator(Declaration* declaration, const Type* type, uint32_t specifiers, bool allow_function_def, const Location& location, bool* last) {
    auto begin = position();
    *last = false;
    if (consume('(')) {
        auto result = parse_declarator(declaration, type, specifiers, allow_function_def, location, last);
        require(')');
        return result;
    } else {
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

        Expr* bit_field_size{};
        if (declaration->scope == IdentifierScope::STRUCTURED && consume(':')) {
            bit_field_size = parse_expr(CONDITIONAL_PREC);
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
            declarator = new Variable(declaration, type, identifier, initializer, bit_field_size, location);
        }

        if (!identifier.name->empty()) {
            if (!symbols.add_declarator(declarator)) {
                return nullptr;
            }
        }

        declarator->fragment = end_fragment(begin);
        return declarator;
    }
}

const Type* Parser::parse_structured_type(Declaration* declaration) {
    auto specifier = token;
    auto specifier_location = preprocessor.location();
    consume();

    Identifier identifier;
    if (token == TOK_IDENTIFIER) {
        identifier = preprocessor.identifier();
        consume();
    }

    const Type* type{};

    if (consume('{')) {
        if (specifier != TOK_ENUM) {
            vector<Declaration*> members;
            while (token && token != '}') {
                auto declaration = dynamic_cast<Declaration*>(parse_declaration_or_statement(IdentifierScope::STRUCTURED));
                assert(declaration);
                members.push_back(declaration);
            }

            if (specifier == TOK_STRUCT) {
                type = new StructType(move(members), specifier_location);
            } else {
                type = new UnionType(move(members), specifier_location);
            }
        } else {
            vector<EnumConstant*> constants;
            while (token && token != '}') {
                auto constant = parse_enum_constant(declaration);
                if (constant) {
                    constants.push_back(constant);
                }
            }

            type = new EnumType(move(constants), specifier_location);
        }

        require('}');
    }

    if (identifier.name->empty()) {
        if (!type) {
            unexpected_token();
            type = IntegerType::default_type();
        }
    } else {
        if (type) {
            auto declarator = new TypeDef(declaration, type, identifier, specifier_location);
            type = new DeclarationType(declarator);

            symbols.add_declarator(declarator);
        } else {
            if (preparse) {
                type = NamedType::of(specifier, identifier);
            } else {
                // TODO
            }
        }
    }

    return type;
}

EnumConstant* Parser::parse_enum_constant(Declaration* declaration) {
    auto location = preprocessor.location();

    auto identifier(token == TOK_IDENTIFIER ? preprocessor.identifier() : Identifier());
    require(TOK_IDENTIFIER);

    Expr* constant{};
    if (consume('=')) {
        constant = parse_expr(CONDITIONAL_PREC);
    }

    if (!consume(',')) {
        if (token != '}') {
            unexpected_token();
        }
    }

    auto declarator = new EnumConstant(declaration, identifier, constant, location);
    if (!symbols.add_declarator(declarator)) {
        return nullptr;
    }

    return declarator;
}
