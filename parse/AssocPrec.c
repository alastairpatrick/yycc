#include "AssocPrec.h"
#include "../lex/Token.h"

const AssocPrec g_assoc_prec[TOK_NUM] = {
    [TOK_EOF]           = { ASSOCIATE_RIGHT, END_PRECEDENCE },
    [';']               = { ASSOCIATE_RIGHT, END_PRECEDENCE },
    [')']               = { ASSOCIATE_RIGHT, END_PRECEDENCE },
    ['}']               = { ASSOCIATE_RIGHT, END_PRECEDENCE },
    [']']               = { ASSOCIATE_RIGHT, END_PRECEDENCE },

    [',']               = { ASSOCIATE_LEFT, SEQUENCE_PRECEDENCE },

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

    ['?']               = { ASSOCIATE_RIGHT, CONDITIONAL_PRECEDENCE },
    [':']               = { ASSOCIATE_RIGHT, CONDITIONAL_PRECEDENCE },

    [TOK_OR_OP]         = { ASSOCIATE_LEFT, LOGICAL_OR_PRECEDENCE },

    [TOK_AND_OP]        = { ASSOCIATE_LEFT, LOGICAL_AND_PRECEDENCE },

    ['|']               = { ASSOCIATE_LEFT, OR_PRECEDENCE },

    ['^']               = { ASSOCIATE_LEFT, EXCLUSIVE_OR_PRECEDENCE },

    ['&']               = { ASSOCIATE_LEFT, AND_PRECEDENCE },

    [TOK_EQ_OP]         = { ASSOCIATE_LEFT, EQUALITY_PRECEDENCE },
    [TOK_NE_OP]         = { ASSOCIATE_LEFT, EQUALITY_PRECEDENCE },

    ['<']               = { ASSOCIATE_LEFT, RELATIONAL_PRECEDENCE },
    ['>']               = { ASSOCIATE_LEFT, RELATIONAL_PRECEDENCE },
    [TOK_LE_OP]         = { ASSOCIATE_LEFT, RELATIONAL_PRECEDENCE },
    [TOK_GE_OP]         = { ASSOCIATE_LEFT, RELATIONAL_PRECEDENCE },

    [TOK_LEFT_OP]       = { ASSOCIATE_LEFT, SHIFT_PRECEDENCE },
    [TOK_RIGHT_OP]      = { ASSOCIATE_LEFT, SHIFT_PRECEDENCE },

    ['+']               = { ASSOCIATE_LEFT, ADDITIVE_PRECEDENCE },
    ['-']               = { ASSOCIATE_LEFT, ADDITIVE_PRECEDENCE },

    ['*']               = { ASSOCIATE_LEFT, MULTIPLICATIVE_PRECEDENCE },
    ['/']               = { ASSOCIATE_LEFT, MULTIPLICATIVE_PRECEDENCE },
    ['%']               = { ASSOCIATE_LEFT, MULTIPLICATIVE_PRECEDENCE },
};

bool is_assignment_token(int token) {
  return token < TOK_NUM && g_assoc_prec[token].is_assignment;
}

