#ifndef NUMFORGE_ROOT_STRATEGY_INTERNAL_H
#define NUMFORGE_ROOT_STRATEGY_INTERNAL_H
#include <numforge/numerical.h>

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
