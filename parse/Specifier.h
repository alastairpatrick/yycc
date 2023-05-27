#ifndef PARSE_SPECIFIER_H
#define PARSE_SPECIFIER_H

#include "lex/Token.h"

typedef uint8_t QualifierSet;
typedef uint64_t SpecifierSet;

inline constexpr SpecifierSet token_to_specifier(TokenKind token) {
    return SpecifierSet(1) << (token - TOK_BEGIN_SPECIFIER_LIKE);
}

inline constexpr bool multiple_specifiers(SpecifierSet specifiers) {
    return specifiers & (specifiers - 1);
}

enum Specifier: SpecifierSet {
    QUALIFIER_CONST = token_to_specifier(TOK_CONST),
    QUALIFIER_RESTRICT = token_to_specifier(TOK_RESTRICT),
    QUALIFIER_VOLATILE = token_to_specifier(TOK_VOLATILE),

    SPECIFIER_AUTO = token_to_specifier(TOK_AUTO),
    SPECIFIER_EXTERN = token_to_specifier(TOK_EXTERN),
    SPECIFIER_REGISTER = token_to_specifier(TOK_REGISTER),
    SPECIFIER_STATIC = token_to_specifier(TOK_STATIC),
    SPECIFIER_TYPEDEF = token_to_specifier(TOK_TYPEDEF),

    SPECIFIER_BOOL = token_to_specifier(TOK_BOOL),
    SPECIFIER_CHAR = token_to_specifier(TOK_CHAR),
    SPECIFIER_COMPLEX = token_to_specifier(TOK_COMPLEX),
    SPECIFIER_FLOAT = token_to_specifier(TOK_FLOAT),
    SPECIFIER_DOUBLE = token_to_specifier(TOK_DOUBLE),
    SPECIFIER_IMAGINARY = token_to_specifier(TOK_IMAGINARY),
    SPECIFIER_INT = token_to_specifier(TOK_INT),
    SPECIFIER_LONG = token_to_specifier(TOK_LONG),
    SPECIFIER_SHORT = token_to_specifier(TOK_SHORT),
    SPECIFIER_SIGNED = token_to_specifier(TOK_SIGNED),
    SPECIFIER_UNSIGNED = token_to_specifier(TOK_UNSIGNED),
    SPECIFIER_VOID = token_to_specifier(TOK_VOID),

    SPECIFIER_ENUM = token_to_specifier(TOK_ENUM),
    SPECIFIER_IDENTIFIER = token_to_specifier(TOK_IDENTIFIER),
    SPECIFIER_STRUCT = token_to_specifier(TOK_STRUCT),
    SPECIFIER_UNION = token_to_specifier(TOK_UNION),

    SPECIFIER_TYPEOF = token_to_specifier(TOK_TYPEOF),
    SPECIFIER_TYPEOF_UNQUAL = token_to_specifier(TOK_TYPEOF_UNQUAL),

    SPECIFIER_INLINE = token_to_specifier(TOK_INLINE),
};

constexpr QualifierSet SPECIFIER_MASK_QUALIFIER = QUALIFIER_CONST | QUALIFIER_RESTRICT | QUALIFIER_VOLATILE;

constexpr SpecifierSet SPECIFIER_MASK_STORAGE_CLASS = SPECIFIER_AUTO | SPECIFIER_EXTERN | SPECIFIER_REGISTER | SPECIFIER_STATIC | SPECIFIER_TYPEDEF;

constexpr SpecifierSet SPECIFIER_MASK_FUNCTION = SPECIFIER_INLINE;

constexpr SpecifierSet SPECIFIER_MASK_TYPE = ~(SPECIFIER_MASK_QUALIFIER | SPECIFIER_MASK_STORAGE_CLASS | SPECIFIER_MASK_FUNCTION);

#endif
