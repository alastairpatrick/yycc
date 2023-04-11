#ifndef LEXER_TOKEN_H
#define LEXER_TOKEN_H

enum TokenKind {
    TOK_EOF,

    TOK_TYPEDEF,
    TOK_EXTERN,
    TOK_STATIC,
    TOK_AUTO,
    TOK_REGISTER,

    TOK_CHAR,
    TOK_SHORT,
    TOK_INT,
    TOK_LONG,
    TOK_SIGNED,
    TOK_UNSIGNED,
    TOK_FLOAT,
    TOK_DOUBLE,
    TOK_VOID,

    TOK_BOOL,
    TOK_COMPLEX,
    TOK_IMAGINARY,
    
    TOK_CONST,
    TOK_RESTRICT,
    TOK_VOLATILE,

    TOK_STRUCT,
    TOK_UNION,
    TOK_ENUM,
    TOK_IDENTIFIER,

    TOK_INLINE,

    // Tokens above are <32 so that they don't overlap with single character punctuators.
    // It is also convenient to be able to use a 32-bit int as a token set for these, e.g.
    // for set of type specifiers.

    // The [32,127] range is reserved for single character punctuators.

    // Tokens below fall in the lower case letter range so they don't overlap with punctuators. 
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
    TOK_CONTINUE,
    TOK_DEFAULT,
    TOK_DO,
    TOK_ELSE,
    TOK_FOR,
    TOK_GOTO,
    TOK_IF,
    TOK_RETURN,
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
    TOK_PP_EXTERN,
    TOK_PP_IF,
    TOK_PP_IFDEF,
    TOK_PP_IFNDEF,
    TOK_PP_INCLUDE,
    TOK_PP_LINE,
    TOK_PP_PRAGMA,
    TOK_PP_STATIC,
    TOK_PP_TYPE,
    TOK_PP_UNDEF,

    TOK_NUM
};

#endif
