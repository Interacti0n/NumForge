#ifndef NUMFORGE_APPLICATION_GRAPH_H
#define NUMFORGE_APPLICATION_GRAPH_H
#include "expression.h"

#define APPLICATION_GRAPH_MAX_ADAPTIVE_SAMPLES 2048U

typedef struct ApplicationGraph
{
    size_t count;
    CalculatorValue *coordinates; /* exact rational x, with a decimal projection */
    ApplicationExpressionSample *samples;
    bool *connect; /* adaptive: midpoint criterion met; no continuity proof */
    BigDecimal *view_minimum, *view_maximum; /* sampled extrema; unresolved poles retain seeds */
    bool limited;
    bool screen_clipped; /* off-screen refinement deferred for an explicit camera */
    char parameter[CALCULATOR_VARIABLE_NAME_BYTES + 1U];
} ApplicationGraph;

typedef enum ApplicationGraphPointKind
{
    APPLICATION_GRAPH_REAL,
    APPLICATION_GRAPH_DOMAIN_ERROR,
    APPLICATION_GRAPH_COMPLEX,
    APPLICATION_GRAPH_QUANTITY,
    APPLICATION_GRAPH_UNRESOLVED /* compact sampler: exact fallback reserve reached */
} ApplicationGraphPointKind;

/* Exact inclusive uniform grid, 2..1024 points, minimum < maximum. Preparation,
 * coordinate generation and all samples share one inherited operation budget.
 * Atomic output, read-only session. No continuity claim or double conversion. */
CalculatorStatus application_graph_sample(ApplicationGraph **result,
    const char *input, const char *parameter, const BigRational *minimum,
    const BigRational *maximum, size_t count, const CalculatorSession *session,
    const CalculatorContext *context, CalculatorError *error);
/* Breadth-first midpoint refinement: 33 seeds, <=2048 samples, depth <=12.
 * parameter=NULL infers it from the AST. All work shares one operation budget.
 * connect flags are heuristic, not mathematical continuity proofs. */
CalculatorStatus application_graph_adaptive(ApplicationGraph **result,
    const char *input, const char *parameter, const BigRational *minimum,
    const BigRational *maximum, const CalculatorSession *session,
    const CalculatorContext *context, CalculatorError *error);
/* Internal bounded-resolution experiment; 33..8192 points. Default adapter
 * uses 2048. Existing memory/time/single-allocation budgets apply. */
CalculatorStatus application_graph_adaptive_limit(ApplicationGraph **result,
    const char *input, const char *parameter, const BigRational *minimum,
    const BigRational *maximum, size_t point_limit, const CalculatorSession *session,
    const CalculatorContext *context, CalculatorError *error);
/* Precision screen fallback: tolerance follows the actual camera, with deferred
 * off-screen gaps. No binary conversion; borrowed bounds, atomic output, same
 * 2048-point/depth/resource limits. Legacy adaptive calls retain seed scaling. */
CalculatorStatus application_graph_adaptive_camera(ApplicationGraph **result,
    const char *input, const char *parameter, const BigRational *minimum,
    const BigRational *maximum, const BigDecimal *y_minimum, const BigDecimal *y_maximum,
    size_t width, size_t height, const CalculatorSession *session,
    const CalculatorContext *context, CalculatorError *error);
ApplicationGraphPointKind application_graph_point_kind(const ApplicationExpressionSample *sample);
void application_graph_destroy(ApplicationGraph *graph);
#endif
