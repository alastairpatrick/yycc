#include "Parser.h"

#include "ArrayType.h"
#include "ASTNode.h"
#include "Constant.h"
#include "Declaration.h"
#include "Expr.h"
#include "lex/Token.h"
#include "Message.h"
#include "Specifier.h"
#include "Statement.h"

const Type* DeclaratorTransform::apply(const Type* type) {
    if (type_transform) type = type_transform(type);
    return type;
}

Parser::Parser(Preprocessor& preprocessor, IdentifierMap& identifiers): preprocessor(preprocessor), identifiers(identifiers), preparse(preprocessor.preparse) {
    assert(identifiers.preparse == preprocessor.preparse);
    consume();
}

Expr* Parser::parse_standalone_expr() {
    return parse_expr(SEQUENCE_PRECEDENCE);
}

Statement* Parser::parse_standalone_statement() {
    return parse_statement();
}

void Parser::consume() {
    while (token) {
        token = preprocessor.next_token();

        switch (token) {
          default:
            return;
          case TOK_PP_ENUM:
          case TOK_PP_FUNC:
          case TOK_PP_TYPE:
          case TOK_PP_VAR:
            handle_declaration_directive();
            break;
        }
    }
}

void Parser::handle_declaration_directive() {
    StorageClass storage_class = token == TOK_PP_ENUM || token == TOK_PP_TYPE ? StorageClass::NONE : StorageClass::EXTERN;

    auto pp_token = preprocessor.next_pp_token();

    while (pp_token && pp_token != '\n') {
        if (pp_token == TOK_IDENTIFIER) {
            auto id = preprocessor.identifier();

            auto declarator = identifiers.add_declarator(AddDeclaratorScope::CURRENT, nullptr, nullptr, id, preprocessor.location());
            if (token == TOK_PP_TYPE) {
                declarator->delegate = new TypeDef(declarator);
                declarator->type = declarator->to_type();
            }
        } else {
            preprocessor.unexpected_directive_token();
        }
        pp_token = preprocessor.next_pp_token();
    }
}

bool Parser::consume(int want_token) {
    if (token != want_token) return false;
    consume();
    return true;
}

bool Parser::require(int want_token) {
    if (token != want_token) {
        if (want_token >= 32 && want_token < 128) {
            auto& stream = message(Severity::ERROR, preprocessor.location()) << "expected '" << (char) want_token << "' but ";
            if (token == TOK_EOF) {
                stream << "reached end of file\n";
            } else {
                stream << "got '" << preprocessor.text() << "'\n";
            }
            pause_messages();
        } else {
            skip_unexpected();
        }
    }

    balance_until(want_token);

    return token == want_token;
}

bool Parser::consume_required(int want_token) {
    if (!require(want_token)) return false;
    consume();
    return true;
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

void Parser::balance_until(int want_token) {
    while (token && token != want_token) {
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

void Parser::skip_expr(OperatorPrec min_prec) {
    // The precedence returned by prec() is wrt the use of the token as closing parenthesis,
    // as a binary operator or as the '?' in the conditional operator. The token might instead
    // be, e.g., a unary operator. The highest precedence token for which there is ambiguity
    // is '&'. Therefore we require that min_prec be such that a misinterpreted '&' would not
    // cause skip_expr to return early.
    assert(min_prec < AND_PRECEDENCE);

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

size_t Parser::position() const {
    return preprocessor.fragment.position;
}

Fragment Parser::end_fragment(size_t begin_position) const {
    return Fragment(begin_position, position() - begin_position);
}

bool Parser::check_eof() {
    if (token == TOK_EOF) return true;
    message(Severity::ERROR, preprocessor.location()) << "expected end of file\n";
    return false;
}

LocationNode* Parser::parse_declaration_or_statement(IdentifierScope scope) {
    resume_messages();

    LocationNode* node = parse_declaration(scope);
    if (node) return node;

    node = parse_statement();
    return node;
}

bool allow_abstract_declarator(IdentifierScope scope) {
    return scope == IdentifierScope::PROTOTYPE || scope == IdentifierScope::STRUCTURED;
}

Declaration* Parser::parse_declaration(IdentifierScope scope) {
    auto location = preprocessor.location();
    auto begin = position();

    const Type* base_type{};
    SpecifierSet specifiers;
    if (auto declaration = parse_declaration_specifiers(scope, base_type, specifiers)) {
        int declarator_count = 0;
        bool last_declarator{};
        while (token && token != ';') {
            ParseDeclaratorFlags flags = { .allow_identifier = true, .allow_initializer = true };
            if (declarator_count == 0) flags.allow_function_definition = true;
            auto declarator = parse_declarator(declaration, base_type, specifiers, flags, &last_declarator);
            if (!declarator->identifier.name->empty() || allow_abstract_declarator(scope)) {
                declaration->declarators.push_back(declarator);
                ++declarator_count;
            }

            // No ';' or ',' after function definition.
            if (last_declarator) break;

            if (!consume(',')) break;
        }

        if (!last_declarator) consume_required(';');

        declaration->fragment = end_fragment(begin);
        return declaration;
    }

    return nullptr;
}

Declaration* Parser::parse_declaration_specifiers(IdentifierScope scope, const Type*& type, SpecifierSet& specifiers) {
    Declaration* declaration{};
    Location declaration_location = preprocessor.location();
    StorageClass storage_class;
    Location storage_class_location;
    Location type_specifier_location = preprocessor.location();
    Location function_specifier_location;
    Location qualifier_location;
    SpecifierSet specifier_set = 0;
    SpecifierSet qualifier_set = 0;
    int num_longs = 0;
    while (token) {
        if (token == TOK_LONG) {
            ++num_longs;
            if (num_longs > 2) {
                message(Severity::ERROR, preprocessor.location()) << "invalid type specifier combination\n";
            }
            specifier_set &= ~token_to_specifier(token);
        }

        Location specifier_location = preprocessor.location();
        TokenKind found_specifier_token{};
        switch (token) {
            case TOK_TYPEDEF:
            case TOK_EXTERN:
            case TOK_STATIC:
            case TOK_AUTO:
            case TOK_REGISTER: {
              found_specifier_token = token;
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
              found_specifier_token = token;
              type_specifier_location = preprocessor.location();                
              consume();
              break;

          } case TOK_IDENTIFIER: {
              if ((specifier_set & SPECIFIER_MASK_TYPE) == 0) {
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
                  found_specifier_token = token;
                  type_specifier_location = preprocessor.location();                
                  consume();
              }
              break;

          } case TOK_ENUM:
            case TOK_STRUCT:
            case TOK_UNION: {
              found_specifier_token = token;
              type_specifier_location = preprocessor.location();
              if (!declaration) declaration = new Declaration(scope, declaration_location);
              type = parse_structured_type(declaration);
              break;

          } case TOK_TYPEOF:
            case TOK_TYPEOF_UNQUAL: {
              found_specifier_token = token;
              type_specifier_location = preprocessor.location();
              type = parse_typeof();
              break;
          
          } case TOK_INLINE: {
              found_specifier_token = token;
              function_specifier_location = preprocessor.location();
              specifier_set &= ~token_to_specifier(token); // function speficiers may be repeated
              consume();
              break;

          } case TOK_CONST:
            case TOK_RESTRICT:
            case TOK_VOLATILE: {
              found_specifier_token = token;
              qualifier_location = preprocessor.location();
              specifier_set &= ~token_to_specifier(token); // qualifiers may be repeated
              consume();
              break;       
          }
        }

        if (found_specifier_token) {
            assert(found_specifier_token >= TOK_BEGIN_SPECIFIER_LIKE && found_specifier_token < TOK_END_SPECIFIER_LIKE);
            if (!declaration) declaration = new Declaration(scope, declaration_location);
            if (specifier_set & token_to_specifier(found_specifier_token)) {
                message(Severity::ERROR, specifier_location) << "invalid declaration specifier or type qualifier combination\n";
            }
            specifier_set |= token_to_specifier(found_specifier_token);
        } else {
            break;
        }
    }

    if (!declaration) return declaration;

    SpecifierSet storage_class_set = specifier_set & SPECIFIER_MASK_STORAGE_CLASS;
    if (storage_class_set != 0 && multiple_specifiers(storage_class_set)) {
        message(Severity::ERROR, storage_class_location) << "too many storage classes\n";
    }

    storage_class = StorageClass::NONE;
    if (storage_class_set & SPECIFIER_STATIC)        storage_class = StorageClass::STATIC;
    else if (storage_class_set & SPECIFIER_EXTERN)   storage_class = StorageClass::EXTERN;
    else if (storage_class_set & SPECIFIER_TYPEDEF)  storage_class = StorageClass::TYPEDEF;
    else if (storage_class_set & SPECIFIER_AUTO)     storage_class = StorageClass::AUTO;
    else if (storage_class_set & SPECIFIER_REGISTER) storage_class = StorageClass::REGISTER;
    
    switch (specifier_set & SPECIFIER_MASK_TYPE) {
        default: {
          type = IntegerType::default_type();
          message(Severity::ERROR, type_specifier_location) << "invalid type specifier combination\n";
          break;

      } case SPECIFIER_VOID: {
          type = &VoidType::it;
          break;

      } case SPECIFIER_CHAR: {
          type = IntegerType::of_char(false);
          break;

      } case SPECIFIER_SIGNED | SPECIFIER_CHAR: {
          type = IntegerType::of(IntegerSignedness::SIGNED, IntegerSize::CHAR);
          break;

      } case SPECIFIER_UNSIGNED | SPECIFIER_CHAR: {
          type = IntegerType::of(IntegerSignedness::UNSIGNED, IntegerSize::CHAR);
          break;

      } case SPECIFIER_SHORT:
        case SPECIFIER_SIGNED | SPECIFIER_SHORT:
        case SPECIFIER_SHORT | SPECIFIER_INT:
        case SPECIFIER_SIGNED | SPECIFIER_SHORT | SPECIFIER_INT: {
          type = IntegerType::of(IntegerSignedness::SIGNED, IntegerSize::SHORT);
          break;

      } case SPECIFIER_UNSIGNED | SPECIFIER_SHORT:
        case SPECIFIER_UNSIGNED | SPECIFIER_SHORT | SPECIFIER_INT: {
          type = IntegerType::of(IntegerSignedness::UNSIGNED, IntegerSize::SHORT);
          break;

      } case SPECIFIER_INT:
        case SPECIFIER_SIGNED:
        case SPECIFIER_SIGNED | SPECIFIER_INT: {
          type = IntegerType::of(IntegerSignedness::SIGNED, IntegerSize::INT);
          break;

      } case SPECIFIER_UNSIGNED:
        case SPECIFIER_UNSIGNED | SPECIFIER_INT: {
          type = IntegerType::of(IntegerSignedness::UNSIGNED, IntegerSize::INT);
          break;

      } case SPECIFIER_LONG:
        case SPECIFIER_SIGNED | SPECIFIER_LONG:
        case SPECIFIER_LONG | SPECIFIER_INT:
        case SPECIFIER_SIGNED | SPECIFIER_LONG | SPECIFIER_INT: {
          type = IntegerType::of(IntegerSignedness::SIGNED, num_longs == 1 ? IntegerSize::LONG : IntegerSize::LONG_LONG);
          break;

      } case SPECIFIER_UNSIGNED | SPECIFIER_LONG:
        case SPECIFIER_UNSIGNED | SPECIFIER_LONG | SPECIFIER_INT: {
          type = IntegerType::of(IntegerSignedness::UNSIGNED, num_longs == 1 ? IntegerSize::LONG : IntegerSize::LONG_LONG);
          break;

      } case SPECIFIER_FLOAT: {
          type = FloatingPointType::of(FloatingPointSize::FLOAT);
          break;

      } case SPECIFIER_DOUBLE: {
          type = FloatingPointType::of(FloatingPointSize::DOUBLE);
          break;

      } case SPECIFIER_LONG | SPECIFIER_DOUBLE: {
          type = FloatingPointType::of(FloatingPointSize::LONG_DOUBLE);
          break;

      } case SPECIFIER_BOOL: {
          type = IntegerType::of(IntegerSignedness::UNSIGNED, IntegerSize::BOOL);
          break;

      } case SPECIFIER_ENUM:
        case SPECIFIER_IDENTIFIER:
        case SPECIFIER_STRUCT:
        case SPECIFIER_UNION:
        case SPECIFIER_TYPEOF:
        case SPECIFIER_TYPEOF_UNQUAL: {
          break;
      }
    }

    if (specifier_set & SPECIFIER_MASK_QUALIFIER) {
        type = QualifiedType::of(type, specifier_set & SPECIFIER_MASK_QUALIFIER);
    }

    declaration->storage_class = storage_class;
    declaration->type = type;

    specifiers = specifier_set & SPECIFIER_MASK_FUNCTION;
    return declaration;
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
            OrderIndependentScope oi_scope;
            oi_scope.position = position();

            // C99 6.7.2.3p6
            if (!identifier.name->empty()) tag_declarator = declare_tag_type(AddDeclaratorScope::CURRENT, declaration, identifier, type, specifier_location);

            structured_type->complete = true;

            if (!anonymous) identifiers.push_scope();

            while (token && token != '}') {
                auto member_declaration = dynamic_cast<Declaration*>(parse_declaration_or_statement(IdentifierScope::STRUCTURED));
                if (!member_declaration) continue;

                // C11 6.7.2.1p13 anonymous structs and unions
                if (dynamic_cast<const StructuredType*>(member_declaration->type) && member_declaration->declarators.empty()) {
                    auto member_declarator = new Declarator(member_declaration, member_declaration->type, Identifier(), member_declaration->location);
                    member_declarator->delegate = new Variable(member_declarator, Linkage::NONE, StorageDuration::AUTO);
                    member_declaration->declarators.push_back(member_declarator);
                }

                structured_type->declarations.push_back(member_declaration);
            }

            if (!anonymous) {
                structured_type->scope = identifiers.pop_scope();

                if (preparse) {
                    oi_scope.scope = &structured_type->scope;
                    order_independent_scopes.push_back(oi_scope);
                }
            }

            structured_type->scope.type = structured_type;

            consume_required('}');
        }
    } else {
        auto enum_type = new EnumType(specifier_location);
        type = enum_type;

        if (consume(':')) {
            const Type* base_type{};
            SpecifierSet specifiers{};
            parse_declaration_specifiers(IdentifierScope::EXPRESSION, base_type, specifiers);

            enum_type->base_type = base_type;
            enum_type->explicit_base_type = true;
        }

        if (consume('{')) {
            // C99 6.7.2.3p6
            if (!identifier.name->empty()) tag_declarator = declare_tag_type(AddDeclaratorScope::CURRENT, declaration, identifier, type, specifier_location);

            enum_type->complete = true;
            while (token && token != '}') {

                auto location = preprocessor.location();

                Identifier identifier;
                if (require(TOK_IDENTIFIER)) {
                    identifier = preprocessor.identifier();
                    consume();
                }

                Expr* constant{};
                if (consume('=')) {
                    constant = parse_expr(CONDITIONAL_PRECEDENCE);
                }

                if (!consume(',')) {
                    if (token != '}') {
                        unexpected_token();
                    }
                }

                auto declarator = identifiers.add_declarator(AddDeclaratorScope::CURRENT, declaration, enum_type, identifier, location);
                auto enum_constant = new EnumConstant(declarator, enum_type, constant);
                declarator->delegate = enum_constant;

                if (declarator) {
                    enum_type->constants.push_back(declarator);
                }
            }

            consume_required('}');
        }
    }

    if (!tag_declarator && !identifier.name->empty()) {
        if (token == ';') {
            // C99 6.7.2.3p7
            tag_declarator = declare_tag_type(AddDeclaratorScope::CURRENT, declaration, identifier, type, specifier_location);
        } else {
            // C99 6.7.2.3p8
            tag_declarator = declare_tag_type(AddDeclaratorScope::FILE, declaration, identifier, type, specifier_location);
        }
    }

    return type;
}

Declarator* Parser::declare_tag_type(AddDeclaratorScope add_scope, Declaration* declaration, const Identifier& identifier, TagType* type, const Location& location) {
    auto declarator = identifiers.add_declarator(add_scope, declaration, type, identifier, location);
    declarator->delegate = new TypeDef(declarator);
    type->tag = declarator;
    return declarator;
}

const Type* Parser::parse_typeof() {
    auto location = preprocessor.location();

    bool keep_qualifiers = token == TOK_TYPEOF;
    consume();
    consume_required('(');

    auto type = parse_type();
    if (!type) {
        auto expr = parse_expr(SEQUENCE_PRECEDENCE);
        type = new TypeOfType(expr, location);
    }

    if (!keep_qualifiers) type = new UnqualifiedType(type);

    consume_required(')');
    return type;
}

Declarator* Parser::parse_declarator(Declaration* declaration, const Type* type, SpecifierSet specifiers, ParseDeclaratorFlags flags, bool* last) {
    auto location = preprocessor.location();
    auto begin = position();

    auto declarator_transform = parse_declarator_transform(declaration->scope, flags);
    if (declarator_transform.identifier.name->empty()) location = declaration->location;

    type = declarator_transform.apply(type);

    bool is_function = declaration->scope != IdentifierScope::PROTOTYPE && dynamic_cast<const FunctionType*>(type);

    Expr* bit_field_size{};
    if (declaration->scope == IdentifierScope::STRUCTURED && consume(':')) {
        bit_field_size = parse_expr(CONDITIONAL_PRECEDENCE);
    }

    Expr* initializer{};
    if (flags.allow_initializer && consume('=')) {
        initializer = parse_initializer();
    }

    bool add_file = declaration->scope == IdentifierScope::FILE || declaration->storage_class == StorageClass::EXTERN;
    bool add_current = declaration->scope != IdentifierScope::FILE;
    AddDeclaratorScope add_scope = add_file && add_current ? AddDeclaratorScope::BOTH : (add_file ? AddDeclaratorScope::FILE : AddDeclaratorScope::CURRENT);
    auto declarator = identifiers.add_declarator(add_scope, declaration, type, declarator_transform.identifier, location);
    
    auto storage_class = declaration->storage_class;
    auto scope = declaration->scope;

    Linkage linkage{};
    if (storage_class == StorageClass::STATIC && scope == IdentifierScope::FILE) {
        linkage = Linkage::INTERNAL;
    } else if (storage_class == StorageClass::EXTERN || scope == IdentifierScope::FILE) {
        linkage = Linkage::EXTERNAL;
    } else {
        linkage = Linkage::NONE;
    }

    if (is_function && declaration->storage_class != StorageClass::TYPEDEF) {
        CompoundStatement* body{};
        if (flags.allow_function_definition && token == '{') {
            identifiers.push_scope(move(declarator_transform.prototype_scope));
            body = parse_compound_statement();
            identifiers.pop_scope();
            *last = true;
        }

        if ((storage_class != StorageClass::STATIC && storage_class != StorageClass::EXTERN && storage_class != StorageClass::NONE) ||
            (storage_class == StorageClass::STATIC && scope != IdentifierScope::FILE)) {
            message(Severity::ERROR, declarator->location) << "invalid storage class\n";
        }

        bool inline_definition = (linkage == Linkage::EXTERNAL) && (specifiers & SPECIFIER_INLINE) && (storage_class != StorageClass::EXTERN);

        declarator->delegate = new Function(declarator,
                                            linkage,
                                            inline_definition,
                                            move(declarator_transform.parameters),
                                            body);
    } else {
        if (specifiers & SPECIFIER_INLINE) {
            message(Severity::ERROR, location) << "'inline' may only appear on function\n";
        }

        if (storage_class == StorageClass::TYPEDEF) {
            declarator->delegate = new TypeDef(declarator);
        } else {
            StorageDuration storage_duration{};
            if (storage_class == StorageClass::EXTERN || storage_class == StorageClass::STATIC || scope == IdentifierScope::FILE) {
                storage_duration = StorageDuration::STATIC;
            } else if (scope == IdentifierScope::STRUCTURED) {
                storage_duration = StorageDuration::MEMBER;
            } else {
                storage_duration = StorageDuration::AUTO;
            }

            declarator->delegate = new Variable(declarator, linkage, storage_duration, initializer, bit_field_size);
        }
    }

    declarator->fragment = end_fragment(begin);
    return declarator;
}

DeclaratorTransform Parser::parse_declarator_transform(IdentifierScope scope, ParseDeclaratorFlags flags) {
    function<const Type*(const Type*)> left_transform;
    while (consume('*')) {
        left_transform = [left_transform](const Type* type) {
            if (left_transform) type = left_transform(type);
            return type->pointer_to();
        };

        SpecifierSet qualifier_set = 0;
        while (token == TOK_CONST || token == TOK_RESTRICT || token == TOK_VOLATILE) {
            qualifier_set |= token_to_specifier(token);
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
        consume_required(')');
    } else {
        if (flags.allow_identifier) {
            if (token == TOK_IDENTIFIER) {
                declarator.identifier = preprocessor.identifier();
                consume();
            } else if (!allow_abstract_declarator(scope)) {
                message(Severity::ERROR, preprocessor.location()) << "expected identifier but got '" << preprocessor.text() << "'\n";
                pause_messages();
            }
        }
    }

    auto location = preprocessor.location();
    function<const Type*(const Type*)> right_transform;
    for (int depth = 0; token; ++depth) {
        if (consume('[')) {
            // C99 6.7.5.3p7
            SpecifierSet array_qualifier_set{};
            if (depth == 0 && scope == IdentifierScope::PROTOTYPE) {
                while (token == TOK_CONST || token == TOK_RESTRICT || token == TOK_VOLATILE || token == TOK_STATIC) {
                    if (token != TOK_STATIC) {
                        array_qualifier_set |= token_to_specifier(token);
                    }
                    consume();
                }
            }

            Expr* array_size{};
            if (token != ']') {
                array_size = parse_expr(ASSIGN_PRECEDENCE);
            }
            consume_required(']');

            right_transform = [right_transform, array_qualifier_set, array_size, location](const Type* type) {
                type = QualifiedType::of(new UnresolvedArrayType(type, array_size, location), array_qualifier_set);
                if (right_transform) type = right_transform(type);
                return type;
            };

        } else if (consume('(')) {
            identifiers.push_scope();

            vector<const Type*> param_types;
            bool seen_void = false;
            if (!consume(')')) {
                while (token) {
                    auto param_declarator = parse_parameter_declarator();
                    if (!param_declarator) {
                        message(Severity::ERROR, preprocessor.location()) << "expected parameter declaration\n";
                        skip_expr(ASSIGN_PRECEDENCE);
                    } else {
                        declarator.parameters.push_back(param_declarator);

                        if (param_declarator->type == &VoidType::it) {
                            if (seen_void || !param_types.empty()) {
                                message(Severity::ERROR, param_declarator->location) << "a parameter may not have void type\n";
                            }
                            seen_void = true;
                        } else {
                            param_types.push_back(param_declarator->type);
                        }
                    }

                    if (!consume(',')) {
                        consume_required(')');
                        break;
                    }
                }
            }

            right_transform = [right_transform, param_types=move(param_types)](const Type* type) {
                type = FunctionType::of(type, move(param_types), false);
                if (right_transform) type = right_transform(type);
                return type;
            };

            declarator.prototype_scope = identifiers.pop_scope();
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

Declarator* Parser::parse_parameter_declarator() {
    auto begin_declaration = position();

    const Type* base_type{};
    SpecifierSet specifiers{};
    auto declaration = parse_declaration_specifiers(IdentifierScope::PROTOTYPE, base_type, specifiers);
    if (!declaration) {
        return nullptr;
    }

    if (declaration->storage_class != StorageClass::NONE && declaration->storage_class != StorageClass::REGISTER) {
        message(Severity::ERROR, declaration->location) << "invalid storage class\n";
        declaration->storage_class = StorageClass::NONE;
    }

    bool last;
    auto begin_declarator = position();
    auto declarator = parse_declarator(declaration, base_type, specifiers, { .allow_identifier = true }, &last);
    declarator->fragment = end_fragment(begin_declarator);

    declaration->declarators.push_back(declarator);
    declaration->fragment = end_fragment(begin_declaration);
    return declarator;
}

Statement* Parser::parse_statement() {
    Location location = preprocessor.location();

    switch (token) {
        case '{': {
          return parse_compound_statement();
      } case TOK_FOR:
        case TOK_WHILE: {
          auto kind = token;
          consume();
          consume_required('(');

          identifiers.push_scope();

          Declaration* declaration{};
          Expr* initialize{};
          Expr* condition{};
          Expr* iterate{};
          if (kind == TOK_FOR) {
              if (!consume(';')) {
                  declaration = parse_declaration(IdentifierScope::BLOCK);
                  if (!declaration) {
                      initialize = parse_expr(SEQUENCE_PRECEDENCE);
                      consume_required(';');
                  }
              }
              if (!consume(';')) {
                  condition = parse_expr(SEQUENCE_PRECEDENCE);
                  consume_required(';');
              }
              if (!consume(')')) {
                  iterate = parse_expr(SEQUENCE_PRECEDENCE);
                  consume_required(')');
              }
          } else {
              condition = parse_expr(SEQUENCE_PRECEDENCE);
              consume_required(')');
          }

          auto body = parse_statement();
          auto statement = new ForStatement(declaration, initialize, condition, iterate, body, location);

          identifiers.pop_scope();

          if (!declaration) return statement;

          ASTNodeVector nodes;
          nodes.push_back(statement);
          return new CompoundStatement(move(nodes), location);

      } case TOK_IF: {
          consume();

          consume_required('(');
          auto condition = parse_expr(SEQUENCE_PRECEDENCE);
          consume_required(')');

          auto then_statement = parse_statement();

          Statement* else_statement{};
          if (consume(TOK_ELSE)) {
              else_statement = parse_statement();
          }

          return new IfElseStatement(condition, then_statement, else_statement, location);

      } case TOK_RETURN: {
          consume();

          Expr* expr{};
          if (token != ';') {
              expr = parse_expr(SEQUENCE_PRECEDENCE);
          }
          consume_required(';');
          return new ReturnStatement(expr, location);

      } case TOK_SWITCH: {
          consume();

          consume_required('(');
          auto expr = parse_expr(SEQUENCE_PRECEDENCE);
          consume_required(')');

          auto statement = new SwitchStatement(expr, nullptr, location);
          auto parent_switch = innermost_switch;
          innermost_switch = statement;

          statement->body = parse_compound_statement();

          innermost_switch = parent_switch;
          return statement;

      } case TOK_CASE:
        case TOK_DEFAULT: {

          bool trouble{};        
          Label label;
          if (consume(TOK_CASE)) {
              label.kind = LabelKind::CASE;
              label.case_expr = parse_expr(CONDITIONAL_PRECEDENCE);
              if (innermost_switch) {
                  innermost_switch->cases.push_back(label.case_expr);
              } else {
                  message(Severity::ERROR, location) << "'case' not within switch statement\n";
                  trouble = true;
              }
          } else {
              consume();
              label.kind = LabelKind::DEFAULT;
              if (innermost_switch) {
                  ++innermost_switch->num_defaults;
              } else {
                  message(Severity::ERROR, location) << "'default' not within switch statement\n";
                  trouble = true;
              }
          }
          consume_required(':');

          auto statement = parse_statement();
          if (!trouble) statement->labels.push_back(label);
          return statement;

      } case TOK_BREAK:
        case TOK_CONTINUE:
        case TOK_GOTO: {
          auto kind = token;
          consume();

          Identifier identifier;
          if (kind == TOK_GOTO) {
              if (require(TOK_IDENTIFIER)) {
                  identifier = preprocessor.identifier();
                  consume();
              }
          }

          consume_required(';');

          return new GoToStatement(kind, identifier, location);

      } default: {
          Label label;
          Statement* statement = parse_expr(SEQUENCE_PRECEDENCE, &label.identifier);

          if (!label.identifier.name->empty()) {
              consume_required(':');

              statement = parse_statement();

              label.kind = LabelKind::GOTO;
              statement->labels.push_back(label);
              return statement;
          }

          consume_required(';');
          return statement;
      }
    }
}

CompoundStatement* Parser::parse_compound_statement() {
    Location location;
    CompoundStatement* statement{};
    if (preparse) {
        require('{');
        location = preprocessor.location();
        consume();

        balance_until('}');
        consume_required('}');

        statement = new CompoundStatement(ASTNodeVector(), location);
    } else {
        identifiers.push_scope();

        require('{');
        location = preprocessor.location();
        consume();

        ASTNodeVector nodes;
        while (token && token != '}') {
            auto node = parse_declaration_or_statement(IdentifierScope::BLOCK);
            nodes.push_back(node);
        }

        Scope scope = identifiers.pop_scope();
        statement = new CompoundStatement(move(nodes), location);

        consume_required('}');
    }

    return statement;
}

Expr* Parser::parse_expr(OperatorPrec min_prec, Identifier* or_label) {
    auto location = preprocessor.location();
    Expr* result{};

    if (preparse) {
        // To make it easier to write tests that run in both preparse and not, decimal integers can always be parsed.
        if (token == TOK_DEC_INT_LITERAL) {
            result = IntegerConstant::of(preprocessor.text(), token, preprocessor.location());
        } else {
            result = IntegerConstant::default_expr(location);
        }

        skip_expr(min_prec);
        return result;
    }

    result = parse_sub_expr(SubExpressionKind::CAST, or_label);
    if (!result) return result;

    while (prec() >= min_prec) {
        location = preprocessor.location();
        auto next_min_prec = assoc() == LEFT_ASSOCIATIVE ? OperatorPrec(prec() + 1) : prec();

        if (consume('?')) {
            auto then_expr = parse_expr(next_min_prec);
            if (consume_required(':')) {
                auto else_expr = parse_expr(CONDITIONAL_PRECEDENCE);
                result = new ConditionExpr(result, then_expr, else_expr, location);
            }
        }
        else {
            TokenKind op;
            switch (token) {
              default:
                return result;
              case '*':
              case '/':
              case '%':
              case '+':
              case '-':
              case TOK_LEFT_OP:
              case TOK_RIGHT_OP:
              case '<':
              case '>':
              case TOK_LE_OP:
              case TOK_GE_OP:
              case TOK_EQ_OP:
              case TOK_NE_OP:
              case '&':
              case '^':
              case '|':
              case TOK_AND_OP:
              case TOK_OR_OP:
              case '=':
              case TOK_MUL_ASSIGN:
              case TOK_DIV_ASSIGN:
              case TOK_MOD_ASSIGN:
              case TOK_ADD_ASSIGN:
              case TOK_SUB_ASSIGN:
              case TOK_LEFT_ASSIGN:
              case TOK_RIGHT_ASSIGN:
              case TOK_AND_ASSIGN:
              case TOK_OR_ASSIGN:
              case TOK_XOR_ASSIGN:
                location = preprocessor.location();
                op = token;
                consume();
                break;
            }

            auto right = parse_expr(next_min_prec);
            result = new BinaryExpr(result, right, op, location);
        }
    }

    return result;
}

Expr* Parser::parse_sub_expr(SubExpressionKind kind, Identifier* or_label) {
    auto location = preprocessor.location();
    Expr* result{};

    switch (token) {
        case TOK_BIN_INT_LITERAL:
        case TOK_OCT_INT_LITERAL:
        case TOK_DEC_INT_LITERAL:
        case TOK_HEX_INT_LITERAL:
        case TOK_CHAR_LITERAL: {
          result = IntegerConstant::of(preprocessor.text(), token, preprocessor.location());
          consume();
          break;

      } case TOK_DEC_FLOAT_LITERAL:
        case TOK_HEX_FLOAT_LITERAL: {
          result = FloatingPointConstant::of(preprocessor.text(), token, preprocessor.location());
          consume();
          break;

      } case TOK_STRING_LITERAL: {
          result = StringConstant::of(preprocessor.text(), preprocessor.location());
          consume();
          break;

      } case TOK_IDENTIFIER: {
          Identifier identifier = preprocessor.identifier();
          consume();

          // This is a hack to parse labels without adding an additional token of lookahead.
          if (or_label && token == ':') {
              *or_label = identifier;
              return nullptr;
          }

          Declarator* declarator = identifiers.lookup_declarator(identifier);
          if (declarator) {
              result = new EntityExpr(declarator, location);
          } else {
              message(Severity::ERROR, location) << '\'' << identifier << "' undeclared\n";
              result = IntegerConstant::default_expr(location);
          }
          break;
      }
    }

    if (!result && kind >= SubExpressionKind::UNARY) {
        switch (token) {
            case '&': {
              consume();
              auto expr = parse_sub_expr(SubExpressionKind::CAST);
              result = new AddressExpr(expr, location);
              break;
            
          } case '*': {
              consume();
              auto expr = parse_sub_expr(SubExpressionKind::CAST);
              result = new DereferenceExpr(expr, location);
              break;

          } case TOK_INC_OP:
            case TOK_DEC_OP: {
              auto op = token;
              consume();
              auto expr = parse_sub_expr(SubExpressionKind::UNARY);
              result = new IncDecExpr(op, expr, false, location);
              break;

          } case TOK_SIZEOF: {
              consume();
              auto consumed_paren = consume('(');
            
              const Type* type{};
              if (consumed_paren) type = parse_type();

              if (type) {
                  result = new SizeOfExpr(type, location);
              } else {
                  auto expr = parse_sub_expr(SubExpressionKind::UNARY);
                  result = new SizeOfExpr(new TypeOfType(expr, location), location);
              }

              if (consumed_paren) consume_required(')');
              break;
          }
        }
    }

    location = preprocessor.location();
    if (!result && consume('(')) {
        if (kind >= SubExpressionKind::CAST) {
            if (auto type = parse_type()) {
                consume_required(')');
                auto expr = parse_sub_expr(SubExpressionKind::CAST);

                if (is_assignment_token(token)) {
                    message(Severity::ERROR, preprocessor.location()) << "cast expression is not assignable\n";
                }

                return new CastExpr(type, expr, location);
            }
        }

        result = parse_expr(SEQUENCE_PRECEDENCE);
        consume_required(')');
    }

    if (!result) {
        unexpected_token();
        result = IntegerConstant::default_expr(preprocessor.location());
        return result;
    }

    while (token) {
        location = preprocessor.location();

        if (consume('[')) {
            auto index = parse_expr(SEQUENCE_PRECEDENCE);
            result = new SubscriptExpr(result, index, location);
            consume_required(']');

        } else if (consume('(')) {
            vector<Expr*> parameters;
            if (token != ')') {
                while (token) {
                    parameters.push_back(parse_expr(ASSIGN_PRECEDENCE));
                    if (!consume(',')) break;
                }
            }
            result = new CallExpr(result, move(parameters), location);
            consume_required(')');

        } else if (token == '.' || token == TOK_PTR_OP) {
            auto op = token;
            consume();
            if (require(TOK_IDENTIFIER)) {
                result = new MemberExpr(op, result, preprocessor.identifier(), location);
                consume();
            }
        } else if (token == TOK_INC_OP || token == TOK_DEC_OP) {
            result = new IncDecExpr(token, result, true, location);
            consume();

        } else {
            break;
        }
    }

    return result;
}

Expr* Parser::parse_initializer() {
    Expr* result{};
    if (consume('{')) {
        if (consume(TOK_VOID)) {
            result = new UninitializedExpr(preprocessor.location());
        } else {
            auto initializer = new InitializerExpr(preprocessor.location());

            while (token && token != '}') {
                initializer->elements.push_back(parse_initializer());
                if (token != '}') consume_required(',');
            }
            
            result = initializer;
        }
        consume_required('}');
    } else {
        result = parse_expr(ASSIGN_PRECEDENCE);
    }

    return result;
}

vector<Declaration*> Parser::parse() {
    vector<Declaration*> declarations;
    while (token) {
        auto keep = !preparse || preprocessor.include_stack.empty();
        auto node = parse_declaration_or_statement(IdentifierScope::FILE);
        auto declaration = dynamic_cast<Declaration*>(node);

        if (declaration) {
            if (keep) {
                declarations.push_back(declaration);
                auto& declarators = identifiers.scopes.back().declarators;
            }
        } else {
            message(Severity::ERROR, node->location) << "expected declaration; statements may occur at block scope but not file scope\n";
        }
    }
    return declarations;
}

OperatorAssoc Parser::assoc() {
    assert(token >= 0 && token < TOK_NUM);
    return g_assoc_prec[token].assoc;
}

OperatorPrec Parser::prec() {
    assert(token >= 0 && token < TOK_NUM);
    return g_assoc_prec[token].prec;
}

const Type* Parser::parse_type() {
    const Type* type{};
    SpecifierSet specifiers{};
    auto declaration = parse_declaration_specifiers(IdentifierScope::EXPRESSION, type, specifiers);
    if (!declaration) return nullptr;

    if (token && token != ')') {
        auto transform = parse_declarator_transform(IdentifierScope::EXPRESSION, {});
        type = transform.apply(type);
    }

    return type;
}
