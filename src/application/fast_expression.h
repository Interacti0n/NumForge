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
    double derivative; /* NAN when unavailable; does not invalidate sample. */
    size_t phase_count,guard_count;
    double phases[64];int phase_kinds[64];
    ApplicationFastGuard guards[64];bool complete,non_real_intermediate;
    size_t exponential_count;
    double exponential_slopes[64]; /* Argument derivatives, even in flat tails. */
    size_t corner_count;
    double corners[64]; /* Inner arguments of abs(); sign brackets locate cusps. */
} ApplicationFastProbe;
/* Restricted continuous display model: sum of up to four affine-amplitude,
 * quadratic-phase sin/cos carriers plus an affine trend. Unsupported trees keep
 * ordinary sampling. This is approximate metadata, not interval proof. */
#define APPLICATION_FAST_DENSE_CARRIERS 4U
typedef struct ApplicationFastDenseCarrier {
    double amplitude,amplitude_offset,phase,phase_offset,phase_square;
    bool cosine;
} ApplicationFastDenseCarrier;
typedef struct ApplicationFastDenseModel {
    double offset,offset_value;
    size_t count;
    bool absolute; /* Only a final abs() around the entire recognized model. */
    ApplicationFastDenseCarrier carriers[APPLICATION_FAST_DENSE_CARRIERS];
} ApplicationFastDenseModel;
CalculatorStatus application_fast_expression_dense_model(const ApplicationFastExpression *expression,
    ApplicationFastDenseModel *result);
CalculatorStatus application_expression_fast_compile(ApplicationFastExpression **result,
    const ApplicationExpression *expression, CalculatorError *error);
void application_fast_expression_destroy(ApplicationFastExpression *expression);
bool application_fast_expression_has_corners(const ApplicationFastExpression *expression);
/* No per-point heap allocations. One shared inherited/default operation budget;
 * structural/resource failure leaves the entire output array untouched. Numeric
 * uncertainty is a per-point request for exact fallback, never a false zero. */
CalculatorStatus application_fast_expression_sample(const ApplicationFastExpression *expression,
    const double *coordinates,size_t count,ApplicationFastSample *result);
/* Same evaluator with bounded trig/domain observations for adaptive edges. */
CalculatorStatus application_fast_expression_probe(const ApplicationFastExpression *expression,
    double coordinate,ApplicationFastProbe *result);
/* Screen-only absolute tail projection. Underflow error must remain below
 * tolerance after supported composition; ordinary probes remain unchanged. */
CalculatorStatus application_fast_expression_probe_screen(const ApplicationFastExpression *expression,
    double coordinate,double tolerance,ApplicationFastProbe *result);
/* Forward-mode AD alongside the same value evaluation and domain observations. */
CalculatorStatus application_fast_expression_probe_derivative(const ApplicationFastExpression *expression,
    double coordinate,ApplicationFastProbe *result);
#endif
