#include "assoc_prec.h"
#include "Token.h"

const AssocPrec g_assoc_prec[0x100] = {
    [TOK_EOF]			= { ASSOC_RIGHT, EOF_PREC },

    // <expression> ::= <assignment-expression>
    //                | <expression> , <assignment-expression>
    [',']				= { ASSOC_LEFT, SEQUENCE_PREC },

    // <assignment-expression> ::= <conditional-expression>
    //                           / <unary-expression> <assignment-operator> <assignment-expression>
    ['=']				= { ASSOC_RIGHT, ASSIGN_PREC },
    [TOK_ADD_ASSIGN]	= { ASSOC_RIGHT, ASSIGN_PREC },
    [TOK_AND_ASSIGN]	= { ASSOC_RIGHT, ASSIGN_PREC },
    [TOK_DIV_ASSIGN]	= { ASSOC_RIGHT, ASSIGN_PREC },
    [TOK_LEFT_ASSIGN]	= { ASSOC_RIGHT, ASSIGN_PREC },
    [TOK_MOD_ASSIGN]	= { ASSOC_RIGHT, ASSIGN_PREC },
    [TOK_MUL_ASSIGN]	= { ASSOC_RIGHT, ASSIGN_PREC },
    [TOK_OR_ASSIGN]		= { ASSOC_RIGHT, ASSIGN_PREC },
    [TOK_RIGHT_ASSIGN]	= { ASSOC_RIGHT, ASSIGN_PREC },
    [TOK_SUB_ASSIGN]	= { ASSOC_RIGHT, ASSIGN_PREC },
    [TOK_XOR_ASSIGN]	= { ASSOC_RIGHT, ASSIGN_PREC },

    // <conditional-expression> ::= <logical-or-expression>
    //                            | <logical-or-expression> ? <expression> : <conditional-expression>
    ['?']				= { ASSOC_RIGHT, CONDITIONAL_PREC },

    // <logical-or-expression> ::= <logical-and-expression>
    //                           | <logical-or-expression> || <logical-and-expression>
    [TOK_OR_OP]			= { ASSOC_LEFT, LOGICAL_OR_PREC },

    // <logical-and-expression> ::= <inclusive-or-expression>
    //                            | <logical-and-expression> && <inclusive-or-expression>
    [TOK_AND_OP]		= { ASSOC_LEFT, LOGICAL_AND_PREC },

    // <inclusive-or-expression> ::= <exclusive-or-expression>
    //                             | <inclusive-or-expression> | <exclusive-or-expression>
    ['|']				= { ASSOC_LEFT, OR_PREC },

    // <exclusive-or-expression> ::= <and-expression>
    //                             | <exclusive-or-expression> ^ <and-expression>
    ['^']				= { ASSOC_LEFT, EXCLUSIVE_OR_PREC },

    // <and-expression> ::= <equality-expression>
    //                    | <and-expression> & <equality-expression>
    ['&']				= { ASSOC_LEFT, AND_PREC },

    // <equality-expression> ::= <relational-expression>
    //                         | <equality-expression> == <relational-expression>
    //                         | <equality-expression> != <relational-expression>
    [TOK_EQ_OP]			= { ASSOC_LEFT, EQUALITY_PREC },
    [TOK_NE_OP]			= { ASSOC_LEFT, EQUALITY_PREC },

    // <relational-expression> ::= <shift-expression>
    //                           | <relational-expression> < <shift-expression>
    //                           | <relational-expression> > <shift-expression>
    //                           | <relational-expression> <= <shift-expression>
    //                           | <relational-expression> >= <shift-expression>
    ['<']				= { ASSOC_LEFT, RELATIONAL_PREC },
    ['>']				= { ASSOC_LEFT, RELATIONAL_PREC },
    [TOK_LE_OP]			= { ASSOC_LEFT, RELATIONAL_PREC },
    [TOK_GE_OP]			= { ASSOC_LEFT, RELATIONAL_PREC },

    // <shift-expression> ::= <additive-expression>
    //                      | <shift-expression> << <additive-expression>
    //                      | <shift-expression> >> <additive-expression>
    [TOK_LEFT_OP]		= { ASSOC_LEFT, SHIFT_PREC },
    [TOK_RIGHT_OP]		= { ASSOC_LEFT, SHIFT_PREC },

    // <additive-expression> ::= <multiplicative-expression>
    //                         | <additive-expression> + <multiplicative-expression>
    //                         | <additive-expression> - <multiplicative-expression>
    ['+']				= { ASSOC_LEFT, ADDITIVE_PREC },
    ['-']				= { ASSOC_LEFT, ADDITIVE_PREC },

    // <multiplicative-expression> ::= <cast-expression>
    //                               | <multiplicative-expression> * <cast-expression>
    //                               | <multiplicative-expression> / <cast-expression>
    //                               | <multiplicative-expression> % <cast-expression>
    ['*']				= { ASSOC_LEFT, MULTIPLICATIVE_PREC },
    ['/']				= { ASSOC_LEFT, MULTIPLICATIVE_PREC },
    ['%']				= { ASSOC_LEFT, MULTIPLICATIVE_PREC },
};
