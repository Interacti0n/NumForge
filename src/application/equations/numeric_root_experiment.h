#ifndef NUMFORGE_NUMERIC_ROOT_EXPERIMENT_H
#define NUMFORGE_NUMERIC_ROOT_EXPERIMENT_H
#include "src/application/equations/numeric_root.h"

/* Private benchmark/test entry point. Production C/HTTP solving still selects
 * bisection. Statistics are published only with a successful diagnostic result.
 * fallback_steps counts main bisections after permanent hybrid switch-off;
 * switch_offs is at most one per request. Rejected proposals count separately. */
typedef struct ApplicationNumericRootStats {
    size_t bisections, interpolations, secants, quadratics, nudges, rejections;
    size_t fallback_steps, switch_offs;
} ApplicationNumericRootStats;
typedef enum ApplicationNumericRootMethod {
    APPLICATION_ROOT_BISECTION,
    APPLICATION_ROOT_RATIONAL_BRENT,
    APPLICATION_ROOT_DECIMAL_HYBRID
} ApplicationNumericRootMethod;
CalculatorStatus application_numeric_root_method(ApplicationNumericRoot **result,
    const char *input, const BigRational *lower, const BigRational *upper,
    const CalculatorContext *context, CalculatorError *error, ApplicationNumericRootMethod method,
    ApplicationNumericRootStats *stats);
#endif
