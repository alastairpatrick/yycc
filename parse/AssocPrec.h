#ifndef PARSE_ASSOCIATE_PREC_H
#define PARSE_ASSOCIATE_PREC_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OP_ASSIGN             = 0x01,
    OP_COMPARISON         = 0x02,
    OP_BOOL_RESULT        = 0x04,
    OP_AS_LEFT_RESULT     = 0x08,
    OP_COMMUTATIVE        = 0x100,

} OperatorFlags;

typedef enum {
    LEFT_ASSOCIATIVE,
    RIGHT_ASSOCIATIVE,
} OperatorAssoc;

typedef enum {
    END_PRECEDENCE = 1,
    SEQUENCE_PRECEDENCE,
    ASSIGN_PRECEDENCE,
    CONDITIONAL_PRECEDENCE,
    LOGICAL_OR_PRECEDENCE,
    LOGICAL_AND_PRECEDENCE,
    OR_PRECEDENCE,
    EXCLUSIVE_OR_PRECEDENCE,
    AND_PRECEDENCE,
    EQUALITY_PRECEDENCE,
    RELATIONAL_PRECEDENCE,
    SHIFT_PRECEDENCE,
    ADDITIVE_PRECEDENCE,
    MULTIPLICATIVE_PRECEDENCE,
} OperatorPrec;

typedef struct {
    OperatorAssoc assoc;
    OperatorPrec prec;
    OperatorFlags op_flags;
} AssocPrec;

extern const AssocPrec g_assoc_prec[];

OperatorFlags operator_flags(int token);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
