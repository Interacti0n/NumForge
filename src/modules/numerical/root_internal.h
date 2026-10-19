#ifndef NUMFORGE_ROOT_INTERNAL_H
#define NUMFORGE_ROOT_INTERNAL_H
#include <numforge/numerical.h>
#include "src/modules/numerical/root_strategy_internal.h"
/* Result storage belongs exclusively to the numerical module. */
struct NumForgeRoot {
    NumForgeRootKind kind;
    BigRational *x, *lower, *upper;
    BigDecimal *residual;
    size_t evaluations, iterations;
    bool exact_zero;
};
struct NumForgeRootScan {
    NumForgeRoot **roots;
    size_t capacity, count, evaluations, iterations, scanned_segments, domain_gaps, rejected_brackets;
    size_t candidate_samples, candidate_roots, candidate_gaps;
    NumForgeRootScanStop stop;
};
#endif
