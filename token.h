#ifndef TOKEN_H
#define TOKEN_H

enum Token {
    TOK_EOF = 0,

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

    // Above must be <32 so they don't overlap single char tokens and so we can make sets of them with a bitmap in a 32-bit int

    TOK_BIN_INT_LITERAL = 0x80,
    TOK_OCT_INT_LITERAL,
    TOK_DEC_INT_LITERAL,
    TOK_HEX_INT_LITERAL,
    TOK_FLOAT_LITERAL,
    TOK_CHAR_LITERAL,
    TOK_STRING_LITERAL,
    TOK_SIZEOF,

    TOK_PTR_OP,
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
    TOK_TYPE_NAME,

    TOK_ELLIPSIS,

    TOK_CASE,
    TOK_DEFAULT,
    TOK_IF,
    TOK_ELSE,
    TOK_SWITCH,
    TOK_WHILE,
    TOK_DO,
    TOK_FOR,
    TOK_GOTO,
    TOK_CONTINUE,
    TOK_BREAK,
    TOK_RETURN,
};

#endif
