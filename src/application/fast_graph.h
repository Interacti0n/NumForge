#ifndef NUMFORGE_APPLICATION_FAST_GRAPH_H
#define NUMFORGE_APPLICATION_FAST_GRAPH_H
#include "graph.h"
#define APPLICATION_FAST_GRAPH_MAX_POINTS 8192U
typedef struct ApplicationFastGraphPoint {
    double x,y;ApplicationGraphPointKind kind;bool connect,approximate,turn_anchor;
    /* turn_anchor only bounds repeated search; derivative retains its value. */
    double derivative;
} ApplicationFastGraphPoint;
typedef struct ApplicationFastGraphBand {double low,high,minimum,maximum;} ApplicationFastGraphBand;
typedef struct ApplicationFastGraph {
    ApplicationFastGraphPoint *points;size_t count,exact_probes;
    ApplicationFastGraphBand *bands;size_t band_count;
    double minimum,maximum;bool limited,screen_clipped,subpixel_hermite,discrete;
    double point_stride;
    char parameter[CALCULATOR_VARIABLE_NAME_BYTES+1U];
} ApplicationFastGraph;
/* Read-only precision evaluation on a bounded integer lattice. automatic=true
 * returns NOT_IMPLEMENTED unless the AST requires integer parameter values.
 * Zero/one samples are valid, no edges are connected, <=256 samples. */
CalculatorStatus application_graph_integer_points(ApplicationFastGraph **result,
    const char *input,const char *parameter,double minimum,double maximum,size_t width,
    bool automatic,const CalculatorSession *session,const CalculatorContext *context,CalculatorError *error);
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
/* Optional approximate pixel envelopes for restricted sin/cos sums with affine
 * amplitudes and affine/quadratic phases, independent of point connections. */
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
/* Opt-in rendered-interpolation validation. A subpixel_hermite result requires
 * its slopes, including below one pixel; legacy callers retain linear checks. */
CalculatorStatus application_fast_graph_sample_camera_hermite(ApplicationFastGraph **result,
    const char *input,const char *parameter,double minimum,double maximum,
    size_t width,size_t height,double y_minimum,double y_maximum,bool coarse,bool dense,
    const CalculatorSession *session,const CalculatorContext *context,CalculatorError *error);
#endif
