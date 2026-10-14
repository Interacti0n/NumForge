#ifndef NUMFORGE_APPLICATION_FAST_GRAPH_H
#define NUMFORGE_APPLICATION_FAST_GRAPH_H
#include "graph.h"
#define APPLICATION_FAST_GRAPH_MAX_POINTS 8192U
typedef struct ApplicationFastGraphPoint {
    double x,y;ApplicationGraphPointKind kind;bool connect,approximate;
    double derivative;
} ApplicationFastGraphPoint;
typedef struct ApplicationFastGraphBand {double low,high,minimum,maximum;} ApplicationFastGraphBand;
typedef struct ApplicationFastGraph {
    ApplicationFastGraphPoint *points;size_t count,exact_probes;
    ApplicationFastGraphBand *bands;size_t band_count;
    double minimum,maximum;bool limited,screen_clipped;
    char parameter[CALCULATOR_VARIABLE_NAME_BYTES+1U];
} ApplicationFastGraph;
/* Approximate screen-space sampler, shared inherited budget and atomic output.
 * NOT_IMPLEMENTED requests the existing exact sampler for unsupported/mostly
 * uncertain trees. Coordinates are finite doubles; no SDK precision claim. */
CalculatorStatus application_fast_graph_sample(ApplicationFastGraph **result,
    const char *input,const char *parameter,double minimum,double maximum,
    size_t width,size_t height,bool coarse,const CalculatorSession *session,
    const CalculatorContext *context,CalculatorError *error);
/* A positive vertical_span sets the camera's y scale; zero retains seed scaling. */
CalculatorStatus application_fast_graph_sample_view(ApplicationFastGraph **result,
    const char *input,const char *parameter,double minimum,double maximum,
    size_t width,size_t height,double vertical_span,bool coarse,const CalculatorSession *session,
    const CalculatorContext *context,CalculatorError *error);
/* Optional approximate pixel envelopes for a restricted single affine sin/cos
 * carrier. Existing point connection flags remain unchanged and independent. */
CalculatorStatus application_fast_graph_sample_view_dense(ApplicationFastGraph **result,
    const char *input,const char *parameter,double minimum,double maximum,
    size_t width,size_t height,double vertical_span,bool coarse,bool dense,
    const CalculatorSession *session,const CalculatorContext *context,CalculatorError *error);
void application_fast_graph_destroy(ApplicationFastGraph *graph);
/* Full camera allows guarded off-screen refinement to be deferred. False edges
 * remain false; screen_clipped results must be resampled for new y coverage. */
CalculatorStatus application_fast_graph_sample_camera(ApplicationFastGraph **result,
    const char *input,const char *parameter,double minimum,double maximum,
    size_t width,size_t height,double y_minimum,double y_maximum,bool coarse,bool dense,
    const CalculatorSession *session,const CalculatorContext *context,CalculatorError *error);
#endif
