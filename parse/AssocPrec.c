#include "AssocPrec.h"
#include "../lex/Token.h"

const AssocPrec g_assoc_prec[TOK_NUM] = {
    [TOK_EOF]           = { ASSOCIATE_RIGHT, END_PRECEDENCE },
    [';']               = { ASSOCIATE_RIGHT, END_PRECEDENCE },
    [')']               = { ASSOCIATE_RIGHT, END_PRECEDENCE },
    ['}']               = { ASSOCIATE_RIGHT, END_PRECEDENCE },
    [']']               = { ASSOCIATE_RIGHT, END_PRECEDENCE },

    // <expression> ::= <assignment-expression>
    //                | <expression> , <assignment-expression>
    [',']               = { ASSOCIATE_LEFT, SEQUENCE_PRECEDENCE },

    // <assignment-expression> ::= <conditional-expression>
    //                           / <unary-expression> <assignment-operator> <assignment-expression>
    ['=']               = { ASSOCIATE_RIGHT, ASSIGN_PRECEDENCE, true },
    [TOK_ADD_ASSIGN]    = { ASSOCIATE_RIGHT, ASSIGN_PRECEDENCE, true },
    [TOK_AND_ASSIGN]    = { ASSOCIATE_RIGHT, ASSIGN_PRECEDENCE, true },
    [TOK_DIV_ASSIGN]    = { ASSOCIATE_RIGHT, ASSIGN_PRECEDENCE, true },
    [TOK_LEFT_ASSIGN]   = { ASSOCIATE_RIGHT, ASSIGN_PRECEDENCE, true },
    [TOK_MOD_ASSIGN]    = { ASSOCIATE_RIGHT, ASSIGN_PRECEDENCE, true },
    [TOK_MUL_ASSIGN]    = { ASSOCIATE_RIGHT, ASSIGN_PRECEDENCE, true },
    [TOK_OR_ASSIGN]     = { ASSOCIATE_RIGHT, ASSIGN_PRECEDENCE, true },
    [TOK_RIGHT_ASSIGN]  = { ASSOCIATE_RIGHT, ASSIGN_PRECEDENCE, true },
    [TOK_SUB_ASSIGN]    = { ASSOCIATE_RIGHT, ASSIGN_PRECEDENCE, true },
    [TOK_XOR_ASSIGN]    = { ASSOCIATE_RIGHT, ASSIGN_PRECEDENCE, true },

    // <conditional-expression> ::= <logical-or-expression>
    //                            | <logical-or-expression> ? <expression> : <conditional-expression>
    ['?']               = { ASSOCIATE_RIGHT, CONDITIONAL_PRECEDENCE },
    [':']               = { ASSOCIATE_RIGHT, CONDITIONAL_PRECEDENCE },

    // <logical-or-expression> ::= <logical-and-expression>
    //                           | <logical-or-expression> || <logical-and-expression>
    [TOK_OR_OP]         = { ASSOCIATE_LEFT, LOGICAL_OR_PRECEDENCE },

    // <logical-and-expression> ::= <inclusive-or-expression>
    //                            | <logical-and-expression> && <inclusive-or-expression>
    [TOK_AND_OP]        = { ASSOCIATE_LEFT, LOGICAL_AND_PRECEDENCE },

    // <inclusive-or-expression> ::= <exclusive-or-expression>
    //                             | <inclusive-or-expression> | <exclusive-or-expression>
    ['|']               = { ASSOCIATE_LEFT, OR_PRECEDENCE },

    // <exclusive-or-expression> ::= <and-expression>
    //                             | <exclusive-or-expression> ^ <and-expression>
    ['^']               = { ASSOCIATE_LEFT, EXCLUSIVE_OR_PRECEDENCE },

    // <and-expression> ::= <equality-expression>
    //                    | <and-expression> & <equality-expression>
    ['&']               = { ASSOCIATE_LEFT, AND_PRECEDENCE },

    // <equality-expression> ::= <relational-expression>
    //                         | <equality-expression> == <relational-expression>
    //                         | <equality-expression> != <relational-expression>
    [TOK_EQ_OP]         = { ASSOCIATE_LEFT, EQUALITY_PRECEDENCE },
    [TOK_NE_OP]         = { ASSOCIATE_LEFT, EQUALITY_PRECEDENCE },

    // <relational-expression> ::= <shift-expression>
    //                           | <relational-expression> < <shift-expression>
    //                           | <relational-expression> > <shift-expression>
    //                           | <relational-expression> <= <shift-expression>
    //                           | <relational-expression> >= <shift-expression>
    ['<']               = { ASSOCIATE_LEFT, RELATIONAL_PRECEDENCE },
    ['>']               = { ASSOCIATE_LEFT, RELATIONAL_PRECEDENCE },
    [TOK_LE_OP]         = { ASSOCIATE_LEFT, RELATIONAL_PRECEDENCE },
    [TOK_GE_OP]         = { ASSOCIATE_LEFT, RELATIONAL_PRECEDENCE },

    // <shift-expression> ::= <additive-expression>
    //                      | <shift-expression> << <additive-expression>
    //                      | <shift-expression> >> <additive-expression>
    [TOK_LEFT_OP]       = { ASSOCIATE_LEFT, SHIFT_PRECEDENCE },
    [TOK_RIGHT_OP]      = { ASSOCIATE_LEFT, SHIFT_PRECEDENCE },

    // <additive-expression> ::= <multiplicative-expression>
    //                         | <additive-expression> + <multiplicative-expression>
    //                         | <additive-expression> - <multiplicative-expression>
    ['+']               = { ASSOCIATE_LEFT, ADDITIVE_PRECEDENCE },
    ['-']               = { ASSOCIATE_LEFT, ADDITIVE_PRECEDENCE },

    // <multiplicative-expression> ::= <cast-expression>
    //                               | <multiplicative-expression> * <cast-expression>
    //                               | <multiplicative-expression> / <cast-expression>
    //                               | <multiplicative-expression> % <cast-expression>
    ['*']               = { ASSOCIATE_LEFT, MULTIPLICATIVE_PRECEDENCE },
    ['/']               = { ASSOCIATE_LEFT, MULTIPLICATIVE_PRECEDENCE },
    ['%']               = { ASSOCIATE_LEFT, MULTIPLICATIVE_PRECEDENCE },
};

bool is_assignment_token(int token) {
  return token < TOK_NUM && g_assoc_prec[token].is_assignment;
}

