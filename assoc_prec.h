#ifndef ASSOC_PREC_H
#define ASSOC_PREC_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ASSOC_LEFT,
    ASSOC_RIGHT,
} OperatorAssoc;

typedef enum {
    EOF_PREC = -1,
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
} AssocPrec;

extern const AssocPrec g_assoc_prec[0x100];

#ifdef __cplusplus
} // extern "C"
#endif

#endif
