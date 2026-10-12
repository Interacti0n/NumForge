#ifndef NUMFORGE_APPLICATION_FAST_EXPRESSION_H
#define NUMFORGE_APPLICATION_FAST_EXPRESSION_H
#include "expression.h"

/* Internal approximate real graph evaluator. Owns copied
 * constants and bytecode; never borrows or modifies the prepared expression.
 * Unsupported syntax returns INVALID_ARGUMENT; caller keeps the exact path. */
typedef struct ApplicationFastExpression ApplicationFastExpression;
typedef enum ApplicationFastKind {
    APPLICATION_FAST_REAL,
    APPLICATION_FAST_NON_REAL,
    APPLICATION_FAST_UNCERTAIN
} ApplicationFastKind;
typedef struct ApplicationFastSample { double value; ApplicationFastKind kind; } ApplicationFastSample;
typedef struct ApplicationFastGuard { double value,exponent; int kind; } ApplicationFastGuard;
typedef struct ApplicationFastProbe {
    ApplicationFastSample sample;
    size_t phase_count,guard_count;
    double phases[64];int phase_kinds[64];
    ApplicationFastGuard guards[64];bool complete;
} ApplicationFastProbe;
CalculatorStatus application_expression_fast_compile(ApplicationFastExpression **result,
    const ApplicationExpression *expression, CalculatorError *error);
void application_fast_expression_destroy(ApplicationFastExpression *expression);
/* No per-point heap allocations. One shared inherited/default operation budget;
 * structural/resource failure leaves the entire output array untouched. Numeric
 * uncertainty is a per-point request for exact fallback, never a false zero. */
CalculatorStatus application_fast_expression_sample(const ApplicationFastExpression *expression,
    const double *coordinates,size_t count,ApplicationFastSample *result);
/* Same evaluator with bounded trig/domain observations for adaptive edges. */
CalculatorStatus application_fast_expression_probe(const ApplicationFastExpression *expression,
    double coordinate,ApplicationFastProbe *result);
#endif
