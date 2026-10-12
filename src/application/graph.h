#ifndef NUMFORGE_APPLICATION_GRAPH_H
#define NUMFORGE_APPLICATION_GRAPH_H
#include "expression.h"

typedef struct ApplicationGraph
{
    size_t count;
    CalculatorValue *coordinates; /* exact rational x, with a decimal projection */
    ApplicationExpressionSample *samples;
    bool *connect; /* adaptive: midpoint criterion met; no continuity proof */
    BigDecimal *view_minimum, *view_maximum; /* seeded real y range, never a domain bound */
    bool limited;
    char parameter[CALCULATOR_VARIABLE_NAME_BYTES + 1U];
} ApplicationGraph;

typedef enum ApplicationGraphPointKind
{
    APPLICATION_GRAPH_REAL,
    APPLICATION_GRAPH_DOMAIN_ERROR,
    APPLICATION_GRAPH_COMPLEX,
    APPLICATION_GRAPH_QUANTITY
} ApplicationGraphPointKind;

/* Exact inclusive uniform grid, 2..1024 points, minimum < maximum. Preparation,
 * coordinate generation and all samples share one inherited operation budget.
 * Atomic output, read-only session. No continuity claim or double conversion. */
CalculatorStatus application_graph_sample(ApplicationGraph **result,
    const char *input, const char *parameter, const BigRational *minimum,
    const BigRational *maximum, size_t count, const CalculatorSession *session,
    const CalculatorContext *context, CalculatorError *error);
/* Breadth-first midpoint refinement: 33 seeds, <=1024 samples, depth <=12.
 * parameter=NULL infers it from the AST. All work shares one operation budget.
 * connect flags are heuristic, not mathematical continuity proofs. */
CalculatorStatus application_graph_adaptive(ApplicationGraph **result,
    const char *input, const char *parameter, const BigRational *minimum,
    const BigRational *maximum, const CalculatorSession *session,
    const CalculatorContext *context, CalculatorError *error);
ApplicationGraphPointKind application_graph_point_kind(const ApplicationExpressionSample *sample);
void application_graph_destroy(ApplicationGraph *graph);
#endif
