#ifndef NUMFORGE_APPLICATION_GRAPH_ANALYSIS_H
#define NUMFORGE_APPLICATION_GRAPH_ANALYSIS_H
#include "expression.h"
#define APPLICATION_GRAPH_MAX_CANDIDATES 32U
typedef enum ApplicationGraphCandidateKind { APPLICATION_GRAPH_ROOT, APPLICATION_GRAPH_VALUE, APPLICATION_GRAPH_INTERSECTION, APPLICATION_GRAPH_POINT_INTERSECTION } ApplicationGraphCandidateKind;
typedef struct ApplicationGraphCandidate { ApplicationGraphCandidateKind kind;double x,low,high,x_tolerance,y_tolerance; } ApplicationGraphCandidate;
typedef struct ApplicationGraphVerifiedPoint { bool confirmed,approximate;double x,y,residual; } ApplicationGraphVerifiedPoint;
typedef struct ApplicationGraphVerificationStats { size_t evaluations,sample_requests,cache_hits; } ApplicationGraphVerificationStats;
/* Numerical verification against the original C expressions. At most 512
 * actual evaluations, 24 refinements per candidate, one inherited time/memory
 * scope, atomic output. Tangency acceptance is numerical, not a proof of roots. */
CalculatorStatus application_graph_verify(ApplicationGraphVerifiedPoint *result,bool *limited,
    const char *input,const char *other,const ApplicationGraphCandidate *candidates,size_t count,
    const CalculatorContext *context,CalculatorError *error);
/* Private diagnostic entry point. Production callers enable reuse; disabling
 * it permits controlled benchmarks under the same numerical/work limits.
 * Optional statistics, like result/limited, are published only on success. */
CalculatorStatus application_graph_verify_profile(ApplicationGraphVerifiedPoint *result,bool *limited,
    const char *input,const char *other,const ApplicationGraphCandidate *candidates,size_t count,
    const CalculatorContext *context,CalculatorError *error,bool reuse,ApplicationGraphVerificationStats *stats);
#endif
