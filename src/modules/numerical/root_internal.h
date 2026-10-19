#ifndef NUMFORGE_ROOT_INTERNAL_H
#define NUMFORGE_ROOT_INTERNAL_H
#include <numforge/numerical.h>
/* Shared only with private application formatting and research adapters. */
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
/* Optional research proposals do not change sampling/acceptance safeguards.
 * used=false requests the production midpoint; reset/accept track the bracket. */
typedef struct NumForgeRootStrategy {
    NumForgeNumericalStatus (*reset)(const BigRational *, const BigRational *,
        const BigDecimal *, const BigDecimal *, void *);
    NumForgeNumericalStatus (*propose)(BigRational *, bool *used, void *);
    NumForgeNumericalStatus (*accept)(const BigRational *, const BigDecimal *, void *);
    void *user;
} NumForgeRootStrategy;
NumForgeNumericalStatus numforge_root_solve_strategy(NumForgeRoot **result,
    NumForgeRootEvaluate evaluate, NumForgeRootDomain domain, void *user,
    const BigRational *lower, const BigRational *upper, const NumForgeRootOptions *options,
    const NumForgeRootStrategy *strategy);
#endif
