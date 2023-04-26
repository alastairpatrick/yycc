#ifndef PARSE_ASSOCIATE_PREC_H
#define PARSE_ASSOCIATE_PREC_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ASSOCIATE_LEFT,
    ASSOCIATE_RIGHT,
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
    bool is_assignment;
} AssocPrec;

extern const AssocPrec g_assoc_prec[];

bool is_assignment_token(int token);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
