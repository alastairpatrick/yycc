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
#include "TranslationUnitContext.h"

const Type* DeclaratorTransform::apply(const Type* type) {
    if (type_transform) type = type_transform(type);
    return type;
}

Parser::Parser(Preprocessor& preprocessor, IdentifierMap& identifiers): preprocessor(preprocessor), identifiers(identifiers), preparse(preprocessor.preparse) {
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
            auto id = preprocessor.identifier;

            auto declarator = identifiers.add_declarator(AddScope::TOP, nullptr, id, nullptr, preprocessor.location());
            if (token == TOK_PP_TYPE) {
                TypeDelegate* type_delegate = new TypeDelegate;
                type_delegate->type_def_type.declarator = declarator->primary;
                declarator->delegate = type_delegate;
                declarator->type = &type_delegate->type_def_type;
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
    return preprocessor.position;
}
bool Parser::check_eof() {
    if (token == TOK_EOF) return true;
    message(Severity::ERROR, preprocessor.location()) << "expected end of file\n";
    return false;
}

LocationNode* Parser::parse_declaration_or_statement(bool expression_valid, ParseDeclaratorFlags flags) {
    resume_messages();

    LocationNode* node = parse_declaration(expression_valid, flags);
    if (node) return node;

    node = parse_statement();
    return node;
}

bool allow_abstract_declarator(ScopeKind scope) {
    return scope == ScopeKind::PROTOTYPE || scope == ScopeKind::STRUCTURED;
}

Declaration* Parser::parse_declaration(bool expression_valid, ParseDeclaratorFlags flags) {
    auto location = preprocessor.location();
    auto begin = position();
    ScopeKind scope = identifiers.scope_kind();

    const Type* base_type{};
    SpecifierSet specifiers;
    if (auto declaration = parse_declaration_specifiers(expression_valid, base_type, specifiers)) {
        int declarator_count = 0;
        bool last_declarator{};
        while (token && token != ';') {
            ParseDeclaratorFlags new_flags = flags;
            if (declarator_count == 0) new_flags.allow_function_definition = true;
            auto declarator = parse_declarator(declaration, base_type, specifiers, new_flags, &last_declarator);
            if (!declarator->identifier->empty() || allow_abstract_declarator(scope)) {
                declaration->declarators.push_back(declarator);
                ++declarator_count;
            }

            // No ';' or ',' after function definition.
            if (last_declarator) break;

            if (!consume(',')) break;
        }

        if (!last_declarator) consume_required(';');

        return declaration;
    }

    return nullptr;
}

static bool is_type_qualifier_token(TokenKind token) {
    return token == TOK_CONST || token == TOK_RESTRICT || token == TOK_TRANSIENT || token == TOK_VOLATILE;
}

Declaration* Parser::parse_declaration_specifiers(bool expression_valid, const Type*& type, SpecifierSet& specifiers) {
    ScopeKind scope = identifiers.scope_kind();

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
                  auto identifier = preprocessor.identifier;
                  if (preparse) {
                      type = UnboundType::of(identifier);
                  } else {
                      auto declarator = identifiers.lookup_declarator(identifier);
                      if (declarator) {
                          auto type_delegate = declarator->type_delegate();
                          type = type_delegate ? &type_delegate->type_def_type : nullptr;
                      }

                      if (!type) {
                          // Not an error if the identifier might be part of an expression instead of a declaration.
                          if (expression_valid) break;

                          auto& stream = message(Severity::ERROR, preprocessor.location()) << "type \'" << *identifier.text << "' ";
                          if (identifier.text != identifier.usage_at_file_scope) {
                              stream << "(aka '" << *identifier.usage_at_file_scope << "') ";
                          }
                          stream << "undefined\n";

                          type = IntegerType::default_type();
                      }
                  }

                  found_specifier_token = token;
                  type_specifier_location = preprocessor.location();                
                  consume();
                  
                  while (consume('.')) {
                      if (!require(TOK_IDENTIFIER)) break;
                      type = new NestedType(type, preprocessor.identifier, preprocessor.location());
                      consume();
                  }
              }
              break;

          } case TOK_ENUM:
            case TOK_STRUCT:
            case TOK_UNION: {
              found_specifier_token = token;
              type_specifier_location = preprocessor.location();
              if (!declaration) declaration = new Declaration(declaration_location);
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
            case TOK_TRANSIENT:
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
            if (!declaration) declaration = new Declaration(declaration_location);
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
          type = IntegerType::of(IntegerSignedness::DEFAULT, IntegerSize::CHAR);
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
    ScopeKind scope = identifiers.scope_kind();

    auto specifier = token;
    auto specifier_location = preprocessor.location();
    consume();

    Identifier identifier;
    if (token == TOK_IDENTIFIER) {
        identifier = preprocessor.identifier;
        consume();
    }

    // C99 6.7.2.3p9
    Declarator* tag_declarator{};
    if (token != ';' && token != '{' && !identifier.empty()) {
        tag_declarator = identifiers.lookup_declarator(identifier);
        if (tag_declarator) {
            return tag_declarator->type;
        }
    }

    bool anonymous = scope == ScopeKind::STRUCTURED && identifier.empty();

    TagType* type{};

    if (specifier != TOK_ENUM) {
        StructuredType* structured_type;
        if (specifier == TOK_STRUCT) {
            structured_type = new StructType(specifier_location);
        } else {
            structured_type = new UnionType(specifier_location);
        }
        type = structured_type;

        if (token == '{') {
            // C99 6.7.2.3p6
            if (!identifier.empty()) tag_declarator = declare_tag_type(AddScope::TOP, declaration, identifier, type, specifier_location);

            structured_type->complete = true;

            if (!anonymous) identifiers.push_scope(new Scope(ScopeKind::STRUCTURED, preprocessor.current_namespace_prefix));
            consume();

            OrderIndependentScope oi_scope;
            oi_scope.position = position();

            while (token && token != '}') {
                auto member_declaration = dynamic_cast<Declaration*>(parse_declaration_or_statement(false, {
                    .allow_identifier = true,
                    .allow_initializer = dynamic_cast<StructType*>(structured_type) != nullptr }));
                if (!member_declaration) continue;

                // C11 6.7.2.1p13 anonymous structs and unions
                if (dynamic_cast<const StructuredType*>(member_declaration->type) && member_declaration->declarators.empty()) {
                    auto variable = new Variable(Linkage::NONE, StorageDuration::AGGREGATE);
                    auto member_declarator = identifiers.add_declarator(AddScope::TOP, member_declaration->type, {}, variable, member_declaration->location);
                    member_declaration->declarators.push_back(member_declarator);
                }

                structured_type->declarations.push_back(member_declaration);
            }

            if (!anonymous) {
                structured_type->scope = identifiers.pop_scope();
                structured_type->scope->type = structured_type;

                if (preparse) {
                    oi_scope.scope = structured_type->scope;
                    order_independent_scopes.push_back(oi_scope);
                }
            }

            consume_required('}');
        }
    } else {
        auto enum_type = new EnumType(specifier_location);
        type = enum_type;

        if (consume(':')) {
            const Type* base_type{};
            SpecifierSet specifiers{};
            parse_declaration_specifiers(false, base_type, specifiers);

            enum_type->base_type = base_type;
            enum_type->explicit_base_type = true;
        }

        if (consume('{')) {
            // C99 6.7.2.3p6
            if (!identifier.empty()) tag_declarator = declare_tag_type(AddScope::TOP, declaration, identifier, type, specifier_location);

            bool first_constant = true;
            Location first_constant_location;
            enum_type->complete = true;
            while (token && token != '}') {

                if (consume('.')) {
                    if (!enum_type->scope) {
                        if (!first_constant) {
                            message(Severity::ERROR, preprocessor.location()) << "'.' must be applied to first enum constant of definition\n";
                            message(Severity::INFO, first_constant_location) << "... see first constant\n";
                        }

                        enum_type->scope = new Scope(ScopeKind::STRUCTURED, preprocessor.current_namespace_prefix);;
                        identifiers.push_scope(enum_type->scope);
                    }
                }

                auto location = preprocessor.location();
                if (first_constant) {
                    first_constant_location = location;
                    first_constant = false;
                }

                Identifier identifier;
                if (require(TOK_IDENTIFIER)) {
                    identifier = preprocessor.identifier;
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

                auto enum_constant = new EnumConstant(enum_type, constant);
                auto declarator = identifiers.add_declarator(AddScope::TOP, enum_type, identifier, enum_constant, location);

                if (declarator) {
                    enum_type->constants.push_back(declarator);
                }
            }

            if (enum_type->scope) {
                identifiers.pop_scope();
            }

            consume_required('}');
        }
    }

    if (!tag_declarator && !identifier.empty()) {
        if (token == ';') {
            // C99 6.7.2.3p7
            tag_declarator = declare_tag_type(AddScope::TOP, declaration, identifier, type, specifier_location);
        } else {
            // C99 6.7.2.3p8
            tag_declarator = declare_tag_type(AddScope::FILE, declaration, identifier, type, specifier_location);
        }
    }

    return type;
}

Declarator* Parser::declare_tag_type(AddScope add_scope, Declaration* declaration, const Identifier& identifier, TagType* type, const Location& location) {
    auto delegate = new TypeDelegate;
    auto declarator = identifiers.add_declarator(add_scope, type, identifier, delegate, location);
    delegate->type_def_type.declarator = declarator->primary;
    type->tag = declarator;
    return declarator;
}

const Type* Parser::parse_typeof() {
    auto location = preprocessor.location();

    bool keep_qualifiers = token == TOK_TYPEOF;
    consume();
    consume_required('(');

    auto type = parse_type(true);
    if (!type) {
        auto expr = parse_expr(SEQUENCE_PRECEDENCE);
        type = new TypeOfType(expr, location);
    }

    if (!keep_qualifiers) type = new UnqualifiedType(type);

    consume_required(')');
    return type;
}

Declarator* Parser::parse_declarator(Declaration* declaration, const Type* type, SpecifierSet specifiers, ParseDeclaratorFlags flags, bool* last) {
    ScopeKind scope = identifiers.scope_kind();

    auto location = preprocessor.location();
    auto begin = position();

    auto declarator_transform = parse_declarator_transform(flags);
    if (declarator_transform.identifier.empty()) location = declaration->location;

    type = declarator_transform.apply(type);

    bool is_function = scope != ScopeKind::PROTOTYPE && dynamic_cast<const FunctionType*>(type);

    Expr* bit_field_size{};
    if (scope == ScopeKind::STRUCTURED && consume(':')) {
        bit_field_size = parse_expr(CONDITIONAL_PRECEDENCE);
    }

    Expr* initializer{};
    if (flags.allow_initializer && consume('=')) {
        initializer = parse_initializer();
    }
    
    auto storage_class = declaration->storage_class;

    Linkage linkage{};
    if (storage_class == StorageClass::STATIC && scope == ScopeKind::FILE) {
        linkage = Linkage::INTERNAL;
    } else if (storage_class == StorageClass::EXTERN || scope == ScopeKind::FILE) {
        linkage = Linkage::EXTERNAL;
    } else {
        linkage = Linkage::NONE;
    }

    DeclaratorDelegate* delegate{};
    AddScope add_scope = AddScope::TOP;
    if (is_function && declaration->storage_class != StorageClass::TYPEDEF) {
        CompoundStatement* body{};
        if (flags.allow_function_definition && token == '{') {
            // todo check there is a prototype scope
            identifiers.push_scope(declarator_transform.prototype_scope);
            body = parse_compound_statement();
            identifiers.pop_scope();
            *last = true;
        }

        if ((storage_class != StorageClass::STATIC && storage_class != StorageClass::EXTERN && storage_class != StorageClass::NONE) ||
            (storage_class == StorageClass::STATIC && scope != ScopeKind::FILE)) {
            message(Severity::ERROR, location) << "invalid storage class\n";
        }

        bool inline_definition = (linkage == Linkage::EXTERNAL) && (specifiers & SPECIFIER_INLINE) && (storage_class != StorageClass::EXTERN);

        delegate = new Function(linkage,
                                inline_definition,
                                move(declarator_transform.parameters),
                                body);

        add_scope = AddScope::TOP;
    } else {
        if (specifiers & SPECIFIER_INLINE) {
            message(Severity::ERROR, location) << "'inline' may only appear on function\n";
        }

        if (storage_class == StorageClass::TYPEDEF) {
            delegate = new TypeDelegate;
        } else {
            StorageDuration storage_duration{};
            if (storage_class == StorageClass::EXTERN || storage_class == StorageClass::STATIC || scope == ScopeKind::FILE) {
                storage_duration = StorageDuration::STATIC;
            } else if (scope == ScopeKind::STRUCTURED) {
                storage_duration = StorageDuration::AGGREGATE;
            } else {
                storage_duration = StorageDuration::AUTO;
            }

            auto variable = new Variable(linkage, storage_duration, initializer);
            delegate = variable;

            if (scope == ScopeKind::STRUCTURED && storage_duration == StorageDuration::AGGREGATE) {
                if (bit_field_size) variable->member->bit_field.reset(new BitField(bit_field_size));
            }
        }
    }

    assert(delegate);

    Declarator* file_declarator{};  // an additional declarator at file scope if appropriate
    Declarator* primary_declarator{};
    if (scope == ScopeKind::BLOCK && declaration->storage_class == StorageClass::EXTERN) {
        file_declarator = identifiers.add_declarator(AddScope::FILE, type, declarator_transform.identifier, delegate, location);
        primary_declarator = file_declarator->primary;
    }

    auto declarator = identifiers.add_declarator(add_scope, type, declarator_transform.identifier, delegate, location, primary_declarator);

    if (auto type_delegate = dynamic_cast<TypeDelegate*>(delegate)) {
        type_delegate->type_def_type.declarator = declarator->primary;
    }

    return declarator;
}

DeclaratorTransform Parser::parse_declarator_transform(ParseDeclaratorFlags flags) {
    ScopeKind scope = identifiers.scope_kind();

    bool is_reference_type{};

    function<const Type*(const Type*)> left_transform;
    while (consume('*')) {
        left_transform = [left_transform](const Type* type) {
            if (left_transform) type = left_transform(type);
            return type->pointer_to();
        };

        SpecifierSet qualifier_set = 0;
        while (is_type_qualifier_token(token)) {
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

    if (flags.allow_reference_type && (token == '&' || token == TOK_AND_OP)) {
        ReferenceType::Kind kind = token == '&' ? ReferenceType::Kind::LVALUE : ReferenceType::Kind::RVALUE;
        is_reference_type = true;
        flags.allow_reference_type = false;
        consume();

        left_transform = [left_transform, kind](const Type* type) {
            if (left_transform) type = left_transform(type);
            return ReferenceType::of(type, kind);
        };
    }
        
    DeclaratorTransform declarator;
    if (consume('(')) {
        declarator = parse_declarator_transform(flags);
        consume_required(')');
    } else {
        if (flags.allow_identifier) {
            if (token == TOK_IDENTIFIER) {
                declarator.identifier = preprocessor.identifier;
                consume();
            } else if (!allow_abstract_declarator(scope)) {
                message(Severity::ERROR, preprocessor.location()) << "expected identifier but got '" << preprocessor.text() << "'\n";
                pause_messages();
            }
        }
    }

    function<const Type*(const Type*)> right_transform;
    for (int depth = 0; token; ++depth) {
        auto location = preprocessor.location();

        if (!is_reference_type && consume('[')) {
            // C99 6.7.5.3p7
            SpecifierSet array_qualifier_set{};
            if (depth == 0 && scope == ScopeKind::PROTOTYPE) {
                while (is_type_qualifier_token(token) || token == TOK_STATIC) {
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
            identifiers.push_scope(new Scope(ScopeKind::PROTOTYPE));

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

            if (consume(TOK_THROW)) {
                auto location = preprocessor.location();
                right_transform = [right_transform, location](const Type* type) {
                    auto throw_type = ThrowType::of(type);
                    const Type* transformed_type = throw_type;
                    if (right_transform) transformed_type = right_transform(transformed_type);

                    if (!dynamic_cast<const FunctionType*>(transformed_type)) {
                        message(Severity::ERROR, location) << "'throw' at wrong position\n";

                        transformed_type = type;
                        if (right_transform) transformed_type = right_transform(transformed_type);
                    }

                    return transformed_type;
                };
            }

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
    auto declaration = parse_declaration_specifiers(false, base_type, specifiers);
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

    declaration->declarators.push_back(declarator);
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

          identifiers.push_scope(new Scope(ScopeKind::BLOCK));

          Declaration* declaration{};
          Expr* initialize{};
          Expr* condition{};
          Expr* iterate{};
          if (kind == TOK_FOR) {
              if (!consume(';')) {
                  ParseDeclaratorFlags flags = { .allow_identifier = true, .allow_initializer = true };
                  declaration = parse_declaration(true, flags);
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
              expr = parse_initializer();
          }
          consume_required(';');

          return new ReturnStatement(expr, location);

      } case TOK_THROW: {
          consume();
          Expr* expr = parse_expr(SEQUENCE_PRECEDENCE);
          consume_required(';');
          return new ThrowStatement(expr, location);

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
                  identifier = preprocessor.identifier;
                  consume();
              }
          }

          consume_required(';');

          return new JumpStatement(kind, identifier, location);

      } case TOK_TRY: {
          consume();
          auto try_statement = parse_compound_statement();
          consume_required(TOK_CATCH);

          identifiers.push_scope(new Scope(ScopeKind::BLOCK));

          consume_required('(');
          auto declarator = parse_parameter_declarator();
          consume_required(')');
          auto catch_statement = parse_compound_statement();

          identifiers.pop_scope();

          return new TryStatement(try_statement, declarator, catch_statement, location);

      } default: {
          Label label;
          auto expr = parse_expr(SEQUENCE_PRECEDENCE, &label.identifier);

          if (!label.identifier.empty()) {
              consume_required(':');

              auto statement = parse_statement();

              label.kind = LabelKind::GOTO;
              statement->labels.push_back(label);
              return statement;
          }

          consume_required(';');
          return new ExprStatement(expr);
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
        identifiers.push_scope(new Scope(ScopeKind::BLOCK));

        require('{');
        location = preprocessor.location();
        consume();

        ASTNodeVector nodes;
        while (token && token != '}') {
            auto node = parse_declaration_or_statement(true, { .allow_identifier = true, .allow_initializer = true });
            if (node) nodes.push_back(node);
        }

        identifiers.pop_scope();
        statement = new CompoundStatement(move(nodes), location);

        consume_required('}');
    }

    return statement;
}

Expr* Parser::parse_expr(OperatorPrec min_prec, Identifier* or_label) {
    auto location = preprocessor.location();
    Expr* expr{};

    if (preparse) {
        // To make it easier to write tests that run in both preparse and not, decimal integers can always be parsed.
        if (token == TOK_DEC_INT_LITERAL) {
            expr = IntegerConstant::of(preprocessor.text(), token, preprocessor.location());
        } else {
            expr = IntegerConstant::default_expr(location);
        }

        skip_expr(min_prec);
        return expr;
    }

    expr = parse_sub_expr(SubExpressionKind::CAST, or_label);
    if (!expr) return expr;

    return continue_parse_expr(expr, min_prec, or_label);
}

Expr* Parser::continue_parse_expr(Expr* expr, OperatorPrec min_prec, Identifier* or_label) {
    assert(!preparse);

    while (prec() >= min_prec) {
        auto location = preprocessor.location();
        auto next_min_prec = assoc() == LEFT_ASSOCIATIVE ? OperatorPrec(prec() + 1) : prec();

        if (consume('?')) {
            auto then_expr = parse_expr(next_min_prec);
            if (consume_required(':')) {
                auto else_expr = parse_expr(CONDITIONAL_PRECEDENCE);
                expr = new ConditionExpr(expr, then_expr, else_expr, location);
            }
        }
        else {
            TokenKind op;
            switch (token) {
              default:
                return expr;
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
              case ',':
                location = preprocessor.location();
                op = token;
                consume();
                break;
            }


            if (op == '=') {
                assert(next_min_prec == ASSIGN_PRECEDENCE);
                auto right = parse_initializer();
                expr = new AssignExpr(expr, right, location);
            } else {
                auto right = parse_expr(next_min_prec);
                if (op ==',') {
                    expr = new SequenceExpr(expr, right, location);
                } else {
                    expr = new BinaryExpr(expr, right, op, location);
                }
            }
        }
    }

    return expr;
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

      } case TOK_FALSE: {
          result = IntegerConstant::of(IntegerType::of_bool(), 0, preprocessor.location());
          consume();
          break;

      } case TOK_TRUE: {
          result = IntegerConstant::of(IntegerType::of_bool(), 1, preprocessor.location());
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
          Identifier identifier = preprocessor.identifier;
          consume();

          // This is a hack to parse labels without adding a token of lookahead.
          if (or_label && token == ':') {
              *or_label = identifier;
              return nullptr;
          }

          result = new EntityExpr(identifiers.top_scope(), identifier, location);
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

          } case TOK_AND_OP: {
              consume();
              auto expr = parse_sub_expr(SubExpressionKind::CAST);
              result = new MoveExpr(expr, location);
              break;

          } case '+':
            case '-':
            case '~':
            case '!': {
              auto op = token;
              consume();
              auto expr = parse_sub_expr(SubExpressionKind::CAST);
              result = new UnaryExpr(expr, op, location);
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
              if (consumed_paren) type = parse_type(true);

              if (type) {
                  result = new SizeOfExpr(type, location);
              } else {
                  Expr* expr{};
                  if (consumed_paren) {
                      expr = parse_expr(SEQUENCE_PRECEDENCE);
                  } else {
                      expr = parse_sub_expr(SubExpressionKind::UNARY);
                  }
                  result = new SizeOfExpr(new TypeOfType(expr, location), location);
              }

              if (consumed_paren) consume_required(')');
              break;
          }
        }
    }

    location = preprocessor.location();
    if (!result && consume('(')) {
        if (kind >= SubExpressionKind::POSTFIX) {
            if (auto type = parse_type(true)) {
                consume_required(')');

                if (token == '{') {
                    result = new CastExpr(type, parse_initializer(), location);
                } else {
                    if (kind < SubExpressionKind::CAST) {
                        message(Severity::ERROR, location) << "cast expression not allowed here\n";
                    }
                    
                    if (token != '.') {
                        auto expr = parse_sub_expr(SubExpressionKind::CAST);

                        if (operator_flags(token) & OP_ASSIGN) {
                            message(Severity::ERROR, preprocessor.location()) << "cast expression is not assignable\n";
                        }

                        result = new CastExpr(type, expr, location);
                    } else {
                        consume();
                        if (require(TOK_IDENTIFIER)) {
                            result = new MemberExpr(TokenKind('.'), type, preprocessor.identifier, preprocessor.location());
                            consume();
                        }
                    }
                }
            }
        }

        if (!result) {
            result = parse_expr(SEQUENCE_PRECEDENCE);
            consume_required(')');
        }
    }

    if (!result) {
        unexpected_token();
        result = IntegerConstant::default_expr(preprocessor.location());
        return result;
    }

    return continue_postfix_expr(result);
}

Expr* Parser::continue_postfix_expr(Expr* expr) {
    while (token) {
        auto location = preprocessor.location();

        if (consume('[')) {
            auto index = parse_expr(SEQUENCE_PRECEDENCE);
            expr = new SubscriptExpr(expr, index, location);
            consume_required(']');

        } else if (consume('(')) {
            vector<Expr*> parameters;
            if (token != ')') {
                while (token) {
                    parameters.push_back(parse_initializer());
                    if (!consume(',')) break;
                }
            }
            expr = new CallExpr(expr, move(parameters), location);
            consume_required(')');

        } else if (token == '.' || token == TOK_PTR_OP) {
            auto op = token;
            consume();
            if (require(TOK_IDENTIFIER)) {
                expr = new MemberExpr(op, expr, preprocessor.identifier, location);
                consume();
            }
        } else if (token == TOK_INC_OP || token == TOK_DEC_OP) {
            expr = new IncDecExpr(token, expr, true, location);
            consume();

        } else {
            break;
        }
    }

    return expr;
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
        auto node = parse_declaration_or_statement(false, { .allow_identifier = true, .allow_initializer = true });
        auto declaration = dynamic_cast<Declaration*>(node);

        if (declaration) {
            declarations.push_back(declaration);
            auto& declarators = identifiers.file_scope()->declarators;
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

const Type* Parser::parse_type(bool expression_valid) {
    const Type* type{};
    SpecifierSet specifiers{};
    auto declaration = parse_declaration_specifiers(expression_valid, type, specifiers);
    if (!declaration) return nullptr;

    if (token && token != ')') {
        auto transform = parse_declarator_transform({});
        type = transform.apply(type);
    }

    return type;
}
