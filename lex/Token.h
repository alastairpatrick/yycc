#ifndef LEX_TOKEN_H
#define LEX_TOKEN_H

enum TokenKind {
    TOK_EOF = 0, // must be zero so it evaluates to false in conditionals
    TOK_INVALID,

    // The [32,127] range is reserved for single character punctuators.

    TOK_PTR_OP = 128,
    TOK_INC_OP,
    TOK_DEC_OP,
    TOK_LEFT_OP,
    TOK_RIGHT_OP,
    TOK_LE_OP,
    TOK_GE_OP,
    TOK_EQ_OP,
    TOK_NE_OP,

    TOK_AND_OP,
    TOK_OR_OP,
    TOK_MUL_ASSIGN,
    TOK_DIV_ASSIGN,
    TOK_MOD_ASSIGN,
    TOK_ADD_ASSIGN,
    TOK_SUB_ASSIGN,
    TOK_LEFT_ASSIGN,
    TOK_RIGHT_ASSIGN,
    TOK_AND_ASSIGN,
    TOK_XOR_ASSIGN,
    TOK_OR_ASSIGN,

    TOK_ELLIPSIS,
    
    TOK_BREAK,
    TOK_CASE,
    TOK_CATCH,
    TOK_CONTINUE,
    TOK_DEFAULT,
    TOK_DO,
    TOK_ELSE,
    TOK_FALSE,
    TOK_FOR,
    TOK_GOTO,
    TOK_IF,
    TOK_RETURN,
    TOK_THROW,
    TOK_TRUE,
    TOK_TRY,
    TOK_SIZEOF,
    TOK_SWITCH,
    TOK_WHILE,

    TOK_CHAR_LITERAL,
    TOK_STRING_LITERAL,
    TOK_HEADER_NAME,
    TOK_BIN_INT_LITERAL,
    TOK_OCT_INT_LITERAL,
    TOK_DEC_INT_LITERAL,
    TOK_HEX_INT_LITERAL,
    TOK_DEC_FLOAT_LITERAL,
    TOK_HEX_FLOAT_LITERAL,

    TOK_PP_NUMBER,
    TOK_PP_CONCAT,
    TOK_PP_UNRECOGNIZED,
    TOK_PP_UNTERMINATED_COMMENT,

    TOK_PP_DEFINE,
    TOK_PP_ELIF,
    TOK_PP_ELSE,
    TOK_PP_EMPTY,
    TOK_PP_ENDIF,
    TOK_PP_ENUM,
    TOK_PP_ERROR,
    TOK_PP_FUNC,
    TOK_PP_IF,
    TOK_PP_IFDEF,
    TOK_PP_IFNDEF,
    TOK_PP_INCLUDE,
    TOK_PP_LINE,
    TOK_PP_NAMESPACE,
    TOK_PP_PRAGMA,
    TOK_PP_TYPE,
    TOK_PP_UNDEF,
    TOK_PP_USING,
    TOK_PP_VAR,

    TOK_BEGIN_SPECIFIER_LIKE,

    // Qualifiers come first so associated set can fit in uint8_t
    TOK_CONST = TOK_BEGIN_SPECIFIER_LIKE,
    TOK_RESTRICT,
    TOK_TRANSIENT,
    TOK_VOLATILE,

    TOK_AUTO,
    TOK_EXTERN,
    TOK_STATIC,
    TOK_REGISTER,
    TOK_TYPEDEF,

    TOK_BOOL,
    TOK_CHAR,
    TOK_COMPLEX,
    TOK_FLOAT,
    TOK_DOUBLE,
    TOK_IMAGINARY,
    TOK_INT,
    TOK_LONG,
    TOK_SHORT,
    TOK_SIGNED,
    TOK_UNSIGNED,
    TOK_VOID,
    
    TOK_ENUM,
    TOK_IDENTIFIER,
    TOK_STRUCT,
    TOK_UNION,

    TOK_TYPEOF,
    TOK_TYPEOF_UNQUAL,

    TOK_INLINE,

    TOK_END_SPECIFIER_LIKE,

    TOK_NUM = TOK_END_SPECIFIER_LIKE
};

#endif
