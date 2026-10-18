#ifndef NUMFORGE_NUMERIC_ROOT_EXPERIMENT_H
#define NUMFORGE_NUMERIC_ROOT_EXPERIMENT_H
#include "numeric_root.h"

/* Private benchmark/test entry point. Production C/HTTP solving still selects
 * bisection. Statistics are published only with a successful diagnostic result. */
typedef struct ApplicationNumericRootStats {
    size_t bisections, interpolations, secants, quadratics, nudges, rejections;
} ApplicationNumericRootStats;
CalculatorStatus application_numeric_root_experiment(ApplicationNumericRoot **result,
    const char *input, const BigRational *lower, const BigRational *upper,
    const CalculatorContext *context, CalculatorError *error, bool brent,
    ApplicationNumericRootStats *stats);
#endif
