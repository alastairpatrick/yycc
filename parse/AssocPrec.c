#include "AssocPrec.h"
#include "../lex/Token.h"

const AssocPrec g_assoc_prec[TOK_NUM] = {
    [TOK_EOF]           = { RIGHT_ASSOCIATIVE, END_PRECEDENCE },
    [';']               = { RIGHT_ASSOCIATIVE, END_PRECEDENCE },
    [')']               = { RIGHT_ASSOCIATIVE, END_PRECEDENCE },
    ['}']               = { RIGHT_ASSOCIATIVE, END_PRECEDENCE },
    [']']               = { RIGHT_ASSOCIATIVE, END_PRECEDENCE },

    [',']               = { LEFT_ASSOCIATIVE, SEQUENCE_PRECEDENCE },

    ['=']               = { RIGHT_ASSOCIATIVE, ASSIGN_PRECEDENCE, OP_ASSIGN | OP_AS_LEFT_RESULT },
    [TOK_ADD_ASSIGN]    = { RIGHT_ASSOCIATIVE, ASSIGN_PRECEDENCE, OP_ASSIGN | OP_AS_LEFT_RESULT },
    [TOK_AND_ASSIGN]    = { RIGHT_ASSOCIATIVE, ASSIGN_PRECEDENCE, OP_ASSIGN | OP_AS_LEFT_RESULT },
    [TOK_DIV_ASSIGN]    = { RIGHT_ASSOCIATIVE, ASSIGN_PRECEDENCE, OP_ASSIGN | OP_AS_LEFT_RESULT },
    [TOK_LEFT_ASSIGN]   = { RIGHT_ASSOCIATIVE, ASSIGN_PRECEDENCE, OP_ASSIGN | OP_AS_LEFT_RESULT },
    [TOK_MOD_ASSIGN]    = { RIGHT_ASSOCIATIVE, ASSIGN_PRECEDENCE, OP_ASSIGN | OP_AS_LEFT_RESULT },
    [TOK_MUL_ASSIGN]    = { RIGHT_ASSOCIATIVE, ASSIGN_PRECEDENCE, OP_ASSIGN | OP_AS_LEFT_RESULT },
    [TOK_OR_ASSIGN]     = { RIGHT_ASSOCIATIVE, ASSIGN_PRECEDENCE, OP_ASSIGN | OP_AS_LEFT_RESULT },
    [TOK_RIGHT_ASSIGN]  = { RIGHT_ASSOCIATIVE, ASSIGN_PRECEDENCE, OP_ASSIGN | OP_AS_LEFT_RESULT },
    [TOK_SUB_ASSIGN]    = { RIGHT_ASSOCIATIVE, ASSIGN_PRECEDENCE, OP_ASSIGN | OP_AS_LEFT_RESULT },
    [TOK_XOR_ASSIGN]    = { RIGHT_ASSOCIATIVE, ASSIGN_PRECEDENCE, OP_ASSIGN | OP_AS_LEFT_RESULT },

    ['?']               = { RIGHT_ASSOCIATIVE, CONDITIONAL_PRECEDENCE },
    [':']               = { RIGHT_ASSOCIATIVE, CONDITIONAL_PRECEDENCE },

    [TOK_OR_OP]         = { LEFT_ASSOCIATIVE, LOGICAL_OR_PRECEDENCE, OP_BOOL_RESULT },

    [TOK_AND_OP]        = { LEFT_ASSOCIATIVE, LOGICAL_AND_PRECEDENCE, OP_BOOL_RESULT },

    ['|']               = { LEFT_ASSOCIATIVE, OR_PRECEDENCE | OP_COMMUTATIVE },

    ['^']               = { LEFT_ASSOCIATIVE, EXCLUSIVE_OR_PRECEDENCE | OP_COMMUTATIVE },

    ['&']               = { LEFT_ASSOCIATIVE, AND_PRECEDENCE | OP_COMMUTATIVE },

    [TOK_EQ_OP]         = { LEFT_ASSOCIATIVE, EQUALITY_PRECEDENCE, OP_COMPARISON | OP_BOOL_RESULT | OP_COMMUTATIVE },
    [TOK_NE_OP]         = { LEFT_ASSOCIATIVE, EQUALITY_PRECEDENCE, OP_COMPARISON | OP_BOOL_RESULT | OP_COMMUTATIVE },

    ['<']               = { LEFT_ASSOCIATIVE, RELATIONAL_PRECEDENCE, OP_COMPARISON | OP_BOOL_RESULT },
    ['>']               = { LEFT_ASSOCIATIVE, RELATIONAL_PRECEDENCE, OP_COMPARISON | OP_BOOL_RESULT },
    [TOK_LE_OP]         = { LEFT_ASSOCIATIVE, RELATIONAL_PRECEDENCE, OP_COMPARISON | OP_BOOL_RESULT },
    [TOK_GE_OP]         = { LEFT_ASSOCIATIVE, RELATIONAL_PRECEDENCE, OP_COMPARISON | OP_BOOL_RESULT },

    [TOK_LEFT_OP]       = { LEFT_ASSOCIATIVE, SHIFT_PRECEDENCE, OP_AS_LEFT_RESULT },
    [TOK_RIGHT_OP]      = { LEFT_ASSOCIATIVE, SHIFT_PRECEDENCE, OP_AS_LEFT_RESULT },

    ['+']               = { LEFT_ASSOCIATIVE, ADDITIVE_PRECEDENCE, OP_COMMUTATIVE },
    ['-']               = { LEFT_ASSOCIATIVE, ADDITIVE_PRECEDENCE },

    ['*']               = { LEFT_ASSOCIATIVE, MULTIPLICATIVE_PRECEDENCE, OP_COMMUTATIVE },
    ['/']               = { LEFT_ASSOCIATIVE, MULTIPLICATIVE_PRECEDENCE },
    ['%']               = { LEFT_ASSOCIATIVE, MULTIPLICATIVE_PRECEDENCE },
};

OperatorFlags operator_flags(int token) {
  if (token < 0 || token >= TOK_NUM) return 0;
  return g_assoc_prec[token].op_flags;
}

