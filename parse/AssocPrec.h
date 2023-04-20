#ifndef PARSE_ASSOC_PREC_H
#define PARSE_ASSOC_PREC_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ASSOC_LEFT,
    ASSOC_RIGHT,
} OperatorAssoc;

typedef enum {
    END_PREC = 1,
    SEQUENCE_PREC,
    ASSIGN_PREC,
    CONDITIONAL_PREC,
    LOGICAL_OR_PREC,
    LOGICAL_AND_PREC,
    OR_PREC,
    EXCLUSIVE_OR_PREC,
    AND_PREC,
    EQUALITY_PREC,
    RELATIONAL_PREC,
    SHIFT_PREC,
    ADDITIVE_PREC,
    MULTIPLICATIVE_PREC,
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
