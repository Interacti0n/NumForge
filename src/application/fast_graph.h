#ifndef NUMFORGE_APPLICATION_FAST_GRAPH_H
#define NUMFORGE_APPLICATION_FAST_GRAPH_H
#include "graph.h"
#define APPLICATION_FAST_GRAPH_MAX_POINTS 8192U
typedef struct ApplicationFastGraphPoint {
    double x,y;ApplicationGraphPointKind kind;bool connect,approximate;
} ApplicationFastGraphPoint;
typedef struct ApplicationFastGraph {
    ApplicationFastGraphPoint *points;size_t count,exact_probes;
    double minimum,maximum;bool limited;
    char parameter[CALCULATOR_VARIABLE_NAME_BYTES+1U];
} ApplicationFastGraph;
/* Approximate screen-space sampler, shared inherited budget and atomic output.
 * NOT_IMPLEMENTED requests the existing exact sampler for unsupported/mostly
 * uncertain trees. Coordinates are finite doubles; no SDK precision claim. */
CalculatorStatus application_fast_graph_sample(ApplicationFastGraph **result,
    const char *input,const char *parameter,double minimum,double maximum,
    size_t width,size_t height,bool coarse,const CalculatorSession *session,
    const CalculatorContext *context,CalculatorError *error);
void application_fast_graph_destroy(ApplicationFastGraph *graph);
#endif
