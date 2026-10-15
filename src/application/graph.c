#include "graph.h"
#include "exact_functions.h"
#include "value_internal.h"
#include "../internal/numforge_alloc.h"
#include <numforge/runtime.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void application_graph_destroy(ApplicationGraph *graph)
{
    if (graph == NULL) return;
    if (graph->coordinates != NULL)
        for (size_t i = 0; i < graph->count; ++i) calculator_value_destroy(&graph->coordinates[i]);
    free(graph->coordinates);
    application_expression_samples_destroy(graph->samples, graph->count);
    free(graph->connect);
    bigdecimal_destroy(graph->view_minimum);bigdecimal_destroy(graph->view_maximum);
    free(graph);
}

ApplicationGraphPointKind application_graph_point_kind(const ApplicationExpressionSample *sample)
{
    if (sample->error.status != CALCULATOR_OK) return APPLICATION_GRAPH_DOMAIN_ERROR;
    if (sample->value.quantity) return APPLICATION_GRAPH_QUANTITY;
    if (calculator_value_is_complex(&sample->value)) return APPLICATION_GRAPH_COMPLEX;
    return APPLICATION_GRAPH_REAL;
}

CalculatorStatus application_graph_sample(ApplicationGraph **result,
    const char *input, const char *parameter, const BigRational *minimum,
    const BigRational *maximum, size_t count, const CalculatorSession *session,
    const CalculatorContext *context, CalculatorError *error)
{
    calculator_error_clear(error);
    if (result == NULL || input == NULL || parameter == NULL || minimum == NULL ||
        maximum == NULL || context == NULL)
    {
        calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0U);
        return CALCULATOR_NULL_ARGUMENT;
    }
    CalculatorStatus status = calculator_context_validate(context, error);
    if (status != CALCULATOR_OK) return status;
    if (count < 2U || count > APPLICATION_EXPRESSION_MAX_SAMPLES)
    {
        calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, 0U);
        return CALCULATOR_INVALID_ARGUMENT;
    }
    bool owner = numforge_budget_begin((uint64_t)context->time_limit_ms,
        CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    ApplicationGraph *graph = NULL;
    ApplicationExpression *expression = NULL;
    BigRational *step = NULL, *divisor = NULL, *cursor = NULL;
    BigDecimal *intervals = NULL;
    int comparison = 0;
    status = calculator_from_rational_status(bigrational_compare(&comparison, minimum, maximum));
    if (status == CALCULATOR_OK && comparison >= 0) status = CALCULATOR_INVALID_ARGUMENT;
    if (status == CALCULATOR_OK)
        status = application_expression_prepare(&expression, input, parameter, session, context, error);
    if (status == CALCULATOR_OK)
    {
        graph = numforge_calloc(1U, sizeof(*graph));
        step = bigrational_create(); divisor = bigrational_create(); cursor = bigrational_create();
        intervals = bigdecimal_create();
        if (graph == NULL || step == NULL || divisor == NULL || cursor == NULL || intervals == NULL)
            status = CALCULATOR_OUT_OF_MEMORY;
    }
    if (status == CALCULATOR_OK)
    {
        graph->count = count;
        graph->coordinates = numforge_calloc(count, sizeof(*graph->coordinates));
        if (graph->coordinates == NULL) status = CALCULATOR_OUT_OF_MEMORY;
    }
    if (status == CALCULATOR_OK)
    {
        char count_text[32];
        (void)snprintf(count_text, sizeof(count_text), "%zu", count - 1U);
        status = calculator_from_decimal_status(bigdecimal_set_string(intervals, count_text));
    }
    if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_from_bigdecimal(divisor, intervals));
    if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_sub(step, maximum, minimum));
    if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_div(step, step, divisor));
    if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_copy(cursor, minimum));
    for (size_t i = 0; status == CALCULATOR_OK && i < count; ++i)
    {
        if (!numforge_budget_check()) { status = CALCULATOR_OUT_OF_MEMORY; break; }
        CalculatorValue *coordinate = &graph->coordinates[i];
        coordinate->kind = CALCULATOR_VALUE_RATIONAL;
        coordinate->context = *context;
        coordinate->independent = true;
        coordinate->rational = bigrational_create(); coordinate->number = bigdecimal_create();
        status = coordinate->rational == NULL || coordinate->number == NULL ? CALCULATOR_OUT_OF_MEMORY :
            calculator_from_rational_status(bigrational_copy(coordinate->rational, cursor));
        if (status == CALCULATOR_OK) status = calculator_materialize_exact(coordinate->number, coordinate, context);
        if (status == CALCULATOR_OK && i + 1U < count)
            status = calculator_from_rational_status(bigrational_add(cursor, cursor, step));
    }
    if (status == CALCULATOR_OK)
        status = application_expression_sample(expression, graph->coordinates, count, &graph->samples, error);
    status = calculator_budget_status(status);
    application_expression_destroy(expression);
    bigrational_destroy(step); bigrational_destroy(divisor); bigrational_destroy(cursor);
    bigdecimal_destroy(intervals);
    if (status == CALCULATOR_OK) *result = graph;
    else
    {
        application_graph_destroy(graph);
        calculator_error_set(error, status, error == NULL ? 0U : error->offset);
    }
    if (owner) numforge_budget_end();
    return status;
}
/* Adaptive plotting uses BigDecimal error estimates, never binary floating point.
 * Legacy requests use seed scaling; screen fallback uses the explicit camera. */
typedef struct GraphRefinement {
    ApplicationGraph *graph;
    ApplicationExpression *expression;
    const CalculatorContext *context;
    CalculatorError *error;
    BigRational *two;
    BigDecimal *half, *sum, *delta, *tolerance;
    BigDecimal *phase_span;
    BigDecimal *coarse_width;
    BigDecimal *strict_phase_span;
    BigDecimal *clip_minimum, *clip_maximum, *camera_tolerance, *pixel_width;
    bool camera;
    bool smooth;
    size_t point_limit;
} GraphRefinement;

/* Deferral never accepts an edge. Three real, ordered probes outside one camera
 * edge are a bounded display heuristic, not proof that a hidden turn is absent.
 * Small x spans and the existing phase checks constrain the deferred region. */
static CalculatorStatus adaptive_offscreen(GraphRefinement *work, size_t left, bool *deferred)
{
    *deferred = false;
    int width = 0, ab = 0, bc = 0;
    CalculatorStatus status = calculator_from_decimal_status(bigdecimal_sub(work->delta,
        work->graph->coordinates[left + 2U].number, work->graph->coordinates[left].number));
    if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_compare(&width, work->delta, work->coarse_width));
    if (status != CALCULATOR_OK || width > 0) return status;
    const BigDecimal *a = work->graph->samples[left].value.number;
    const BigDecimal *b = work->graph->samples[left + 1U].value.number;
    const BigDecimal *c = work->graph->samples[left + 2U].value.number;
    status = calculator_from_decimal_status(bigdecimal_compare(&ab, a, b));
    if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_compare(&bc, b, c));
    if (status != CALCULATOR_OK || !((ab <= 0 && bc <= 0) || (ab >= 0 && bc >= 0))) return status;
    bool below = true, above = true;
    for (size_t i = left; status == CALCULATOR_OK && i <= left + 2U; ++i) {
        int low = 0, high = 0;
        status = calculator_from_decimal_status(bigdecimal_compare(&low, work->graph->samples[i].value.number, work->clip_minimum));
        if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_compare(&high, work->graph->samples[i].value.number, work->clip_maximum));
        below = below && low < 0;above = above && high > 0;
    }
    if (status == CALCULATOR_OK) *deferred = below || above;
    return status;
}

static CalculatorStatus adaptive_coordinate(CalculatorValue *value,
    const BigRational *rational, const CalculatorContext *context)
{
    value->kind = CALCULATOR_VALUE_RATIONAL;value->context = *context;value->independent = true;
    value->rational = bigrational_create();value->number = bigdecimal_create();
    if (value->rational == NULL || value->number == NULL) return CALCULATOR_OUT_OF_MEMORY;
    CalculatorStatus status = calculator_from_rational_status(bigrational_copy(value->rational, rational));
    if (status == CALCULATOR_OK) status = calculator_materialize_exact(value->number, value, context);
    return status;
}

static CalculatorStatus adaptive_view(ApplicationGraph *graph, bool *any)
{
    *any = false;
    for (size_t i = 0; i < graph->count; ++i) {
        if (application_graph_point_kind(&graph->samples[i]) != APPLICATION_GRAPH_REAL) continue;
        const BigDecimal *y = graph->samples[i].value.number;
        CalculatorStatus status = CALCULATOR_OK;
        if (!*any) {
            status = calculator_from_decimal_status(bigdecimal_copy(graph->view_minimum, y));
            if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_copy(graph->view_maximum, y));
        } else {
            int comparison = 0;
            status = calculator_from_decimal_status(bigdecimal_compare(&comparison, y, graph->view_minimum));
            if (status == CALCULATOR_OK && comparison < 0) status = calculator_from_decimal_status(bigdecimal_copy(graph->view_minimum, y));
            if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_compare(&comparison, y, graph->view_maximum));
            if (status == CALCULATOR_OK && comparison > 0) status = calculator_from_decimal_status(bigdecimal_copy(graph->view_maximum, y));
        }
        if (status != CALCULATOR_OK) return status;
        *any = true;
    }
    return CALCULATOR_OK;
}

/* One probe; breadth-first scheduling below distributes work over the viewport. */
static CalculatorStatus adaptive_interval(GraphRefinement *work, size_t left,
    bool *accepted, bool *empty)
{
    ApplicationGraph *graph = work->graph;
    *accepted = false;*empty = false;
    if (!numforge_budget_check()) return calculator_budget_status(CALCULATOR_OUT_OF_MEMORY);
    CalculatorValue midpoint = {0};
    BigRational *middle = bigrational_create();
    ApplicationExpressionSample *evaluated = NULL;
    CalculatorStatus status = middle == NULL ? CALCULATOR_OUT_OF_MEMORY :
        calculator_from_rational_status(bigrational_add(middle, graph->coordinates[left].rational, graph->coordinates[left + 1U].rational));
    if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_div(middle, middle, work->two));
    if (status == CALCULATOR_OK) status = adaptive_coordinate(&midpoint, middle, work->context);
    if (status == CALCULATOR_OK) status = application_expression_sample(work->expression, &midpoint, 1U, &evaluated, work->error);
    bigrational_destroy(middle);
    if (status != CALCULATOR_OK) { calculator_value_destroy(&midpoint);application_expression_samples_destroy(evaluated, 1U);return status; }
    size_t tail = graph->count - left - 1U;
    memmove(graph->coordinates + left + 2U, graph->coordinates + left + 1U, tail * sizeof(*graph->coordinates));
    memmove(graph->samples + left + 2U, graph->samples + left + 1U, tail * sizeof(*graph->samples));
    memmove(graph->connect + left + 2U, graph->connect + left + 1U, tail * sizeof(*graph->connect));
    graph->coordinates[left + 1U] = midpoint;
    graph->samples[left + 1U] = evaluated[0];free(evaluated);
    graph->connect[left + 1U] = false;++graph->count;
    graph->connect[left + 2U] = false;
    bool a = application_graph_point_kind(&graph->samples[left]) == APPLICATION_GRAPH_REAL;
    bool b = application_graph_point_kind(&graph->samples[left + 1U]) == APPLICATION_GRAPH_REAL;
    bool c = application_graph_point_kind(&graph->samples[left + 2U]) == APPLICATION_GRAPH_REAL;
    if (!a && !b && !c) { *empty = true;return CALCULATOR_OK; }
    if (a && b && c) {
        status = calculator_from_decimal_status(bigdecimal_add(work->sum, graph->samples[left].value.number, graph->samples[left + 2U].value.number));
        if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_mul(work->sum, work->sum, work->half));
        if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_sub(work->delta, graph->samples[left + 1U].value.number, work->sum));
        if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_abs(work->delta, work->delta));
        int comparison = 0;
        if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_compare(&comparison, work->delta, work->tolerance));
        int camera_comparison = 1;
        if (status == CALCULATOR_OK && work->camera) status = calculator_from_decimal_status(bigdecimal_compare(&camera_comparison, work->delta, work->camera_tolerance));
        if (status != CALCULATOR_OK) return status;
        bool phases_safe = true, phases_tight = true;
        for (size_t i = left; i <= left + 2U; ++i)
            if (graph->samples[i].phase_unknown || graph->samples[i].phase_count != graph->samples[left].phase_count) phases_safe = false;
        /* Small phase steps avoid accepting a chord that hides whole periods.
         * This is a sampled safeguard, not a proof of continuity. */
        for (size_t phase = 0; phases_safe && phase < graph->samples[left].phase_count; ++phase)
            for (size_t i = left; phases_safe && i < left + 2U; ++i)
                for (size_t j = i + 1U; phases_safe && j <= left + 2U; ++j) {
                    const BigDecimal *p = graph->samples[i].phases[phase], *q = graph->samples[j].phases[phase];
                    if (p == NULL || q == NULL) { phases_safe = false;break; }
                    status = calculator_from_decimal_status(bigdecimal_sub(work->delta, p, q));
                    if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_abs(work->delta, work->delta));
                    int phase_comparison = 0;
                    if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_compare(&phase_comparison, work->delta, work->phase_span));
                    if (status != CALCULATOR_OK) return status;
                    if (phase_comparison > 0) phases_safe = false;
                    if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_compare(&phase_comparison,work->delta,work->strict_phase_span));
                    if (status != CALCULATOR_OK) return status;
                    if (phase_comparison > 0) phases_tight = false;
                }
        bool coarse = false;
        if (work->camera && comparison > 0 && phases_safe && phases_tight) {
            bool deferred = false;
            status = adaptive_offscreen(work, left, &deferred);
            if (status != CALCULATOR_OK) return status;
            if (deferred) { graph->screen_clipped = true;*empty = true;return CALCULATOR_OK; }
        }
        /* Display approximation for a restricted continuous AST: a parent
         * narrower than 1/256 of the view contributes two short chords.
         * Domain/pole/jump operations keep the original strict criterion. */
        if (!work->camera && work->smooth && phases_safe && (comparison > 0 || !phases_tight)) {
            status = calculator_from_decimal_status(bigdecimal_sub(work->delta,
                graph->coordinates[left + 2U].number, graph->coordinates[left].number));
            int width_comparison = 0;
            if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_compare(&width_comparison,work->delta,work->coarse_width));
            if (status != CALCULATOR_OK) return status;
            coarse = width_comparison <= 0;
        }
        /* Preserve small seed signals while searching large intervals, then
         * avoid refining a discovered smooth feature beyond screen needs.
         * Unlike the legacy floor, both pixel width AND camera error must pass. */
        if (work->camera && work->smooth && phases_safe && phases_tight && camera_comparison <= 0) {
            int width_comparison = 0;
            status = calculator_from_decimal_status(bigdecimal_sub(work->delta,
                graph->coordinates[left + 2U].number, graph->coordinates[left].number));
            if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_compare(&width_comparison, work->delta, work->pixel_width));
            if (status != CALCULATOR_OK) return status;
            coarse = width_comparison <= 0;
        }
        if (((comparison <= 0 && phases_tight) || coarse) && phases_safe) {
            graph->connect[left + 1U] = true;graph->connect[left + 2U] = true;*accepted = true;
        }
    }
    return status;
}

/* Add actual evaluations at sin/cos phase extrema on already accepted edges.
 * Interpolation locates an initial candidate; bounded bracket refinement checks
 * its captured C phase, so no synthetic +/-1 values are put into the graph. */
static CalculatorStatus adaptive_peaks(GraphRefinement *work)
{
    ApplicationGraph *graph=work->graph;
    if(!work->smooth || graph->samples[0].phase_count==0U)return CALCULATOR_OK;
    BigDecimal *period=bigdecimal_create(),*offset=bigdecimal_create(),*target=bigdecimal_create();
    BigDecimal *index=bigdecimal_create(),*delta=bigdecimal_create(),*ratio=bigdecimal_create();
    BigDecimal *a=bigdecimal_create(),*b=bigdecimal_create(),*epsilon=bigdecimal_create(),*one=bigdecimal_create();
    BigRational *lo=bigrational_create(),*hi=bigrational_create(),*fraction=bigrational_create(),*position=bigrational_create();
    CalculatorStatus status=CALCULATOR_OK;
    if(!period||!offset||!target||!index||!delta||!ratio||!a||!b||!epsilon||!one||!lo||!hi||!fraction||!position)status=CALCULATOR_OUT_OF_MEMORY;
    int64_t digits=work->context->division_scale<34?34:work->context->division_scale;
#define PEAK_TRY(op) do { status=calculator_from_decimal_status(op);if(status!=CALCULATOR_OK)goto cleanup; } while(0)
#define PEAK_RAT(op) do { status=calculator_from_rational_status(op);if(status!=CALCULATOR_OK)goto cleanup; } while(0)
    if(status!=CALCULATOR_OK)goto cleanup;
    if(work->context->angle_unit==CALCULATOR_ANGLE_DEGREES) { PEAK_TRY(bigdecimal_set_string(period,"180")); }
    else { PEAK_TRY(bigdecimal_set_constant_significant(period,BIGDECIMAL_CONSTANT_PI,digits+16,BIGDECIMAL_ROUND_HALF_EVEN)); }
    PEAK_TRY(bigdecimal_set_string(epsilon,"0.000001"));PEAK_TRY(bigdecimal_set_string(one,"1"));
    for(size_t edge=1U;edge<graph->count;++edge) {
        if(!graph->connect[edge])continue;
        for(size_t phase=0U;phase<graph->samples[edge-1U].phase_count;++phase) {
            int kind=application_expression_graph_peak_offset(work->expression,phase);
            if(kind<0 || graph->samples[edge-1U].phase_unknown || graph->samples[edge].phase_unknown)continue;
            const BigDecimal *p=graph->samples[edge-1U].phases[phase],*q=graph->samples[edge].phases[phase];
            if(!p||!q)continue;
            int direction=0;PEAK_TRY(bigdecimal_compare(&direction,p,q));if(direction==0)continue;
            if(kind) { PEAK_TRY(bigdecimal_mul(offset,period,work->half)); }
            else { PEAK_TRY(bigdecimal_set_string(offset,"0")); }
            PEAK_TRY(bigdecimal_sub(delta,direction<0?p:q,offset));
            PEAK_TRY(bigdecimal_div_significant(index,delta,period,digits,BIGDECIMAL_ROUND_HALF_EVEN));
            PEAK_TRY(bigdecimal_floor(index,index));PEAK_TRY(bigdecimal_add(index,index,one));
            PEAK_TRY(bigdecimal_mul(target,index,period));PEAK_TRY(bigdecimal_add(target,target,offset));
            int inside=0;PEAK_TRY(bigdecimal_compare(&inside,target,direction<0?q:p));if(inside>=0)continue;
            /* Existing endpoint anchors within a micro-radian need no duplicate. */
            PEAK_TRY(bigdecimal_sub(delta,target,p));PEAK_TRY(bigdecimal_abs(delta,delta));PEAK_TRY(bigdecimal_compare(&inside,delta,epsilon));if(inside<=0)continue;
            PEAK_TRY(bigdecimal_sub(delta,target,q));PEAK_TRY(bigdecimal_abs(delta,delta));PEAK_TRY(bigdecimal_compare(&inside,delta,epsilon));if(inside<=0)continue;
            PEAK_TRY(bigdecimal_copy(a,p));PEAK_TRY(bigdecimal_copy(b,q));
            PEAK_RAT(bigrational_copy(lo,graph->coordinates[edge-1U].rational));PEAK_RAT(bigrational_copy(hi,graph->coordinates[edge].rational));
            for(unsigned attempt=0U;attempt<8U;++attempt) {
                if(graph->count>=work->point_limit || numforge_budget_available_bytes()<3U*1024U*1024U || numforge_budget_available_ms()<300U) {graph->limited=true;goto cleanup;}
                PEAK_TRY(bigdecimal_sub(delta,b,a));PEAK_TRY(bigdecimal_sub(ratio,target,a));
                PEAK_TRY(bigdecimal_div_significant(ratio,ratio,delta,digits,BIGDECIMAL_ROUND_HALF_EVEN));
                PEAK_RAT(bigrational_from_bigdecimal(fraction,ratio));PEAK_RAT(bigrational_sub(position,hi,lo));
                PEAK_RAT(bigrational_mul(position,position,fraction));PEAK_RAT(bigrational_add(position,position,lo));
                CalculatorValue coordinate={0};ApplicationExpressionSample *sample=NULL;
                status=adaptive_coordinate(&coordinate,position,work->context);
                if(status==CALCULATOR_OK)status=application_expression_sample(work->expression,&coordinate,1U,&sample,work->error);
                if(status!=CALCULATOR_OK){calculator_value_destroy(&coordinate);application_expression_samples_destroy(sample,1U);goto cleanup;}
                bool usable=application_graph_point_kind(sample)==APPLICATION_GRAPH_REAL && !sample->phase_unknown && phase<sample->phase_count && sample->phases[phase]!=NULL;
                int close=1,side=0;
                if(usable) {
                    status=calculator_from_decimal_status(bigdecimal_sub(delta,sample->phases[phase],target));
                    if(status==CALCULATOR_OK)status=calculator_from_decimal_status(bigdecimal_compare(&side,sample->phases[phase],target));
                    if(status==CALCULATOR_OK)status=calculator_from_decimal_status(bigdecimal_abs(delta,delta));
                    if(status==CALCULATOR_OK)status=calculator_from_decimal_status(bigdecimal_compare(&close,delta,epsilon));
                }
                if(status==CALCULATOR_OK && usable && close<=0) {
                    size_t tail=graph->count-edge;
                    memmove(graph->coordinates+edge+1U,graph->coordinates+edge,tail*sizeof(*graph->coordinates));
                    memmove(graph->samples+edge+1U,graph->samples+edge,tail*sizeof(*graph->samples));
                    memmove(graph->connect+edge+1U,graph->connect+edge,tail*sizeof(*graph->connect));
                    graph->coordinates[edge]=coordinate;graph->samples[edge]=sample[0];free(sample);
                    graph->connect[edge]=true;++graph->count;++edge;break;
                }
                if(status==CALCULATOR_OK && usable) {
                    bool lower=direction<0?side<0:side>0;
                    status=calculator_from_decimal_status(bigdecimal_copy(lower?a:b,sample->phases[phase]));
                    if(status==CALCULATOR_OK)status=calculator_from_rational_status(bigrational_copy(lower?lo:hi,position));
                }
                calculator_value_destroy(&coordinate);application_expression_samples_destroy(sample,1U);
                if(status!=CALCULATOR_OK)goto cleanup;
                if(!usable)break;
            }
        }
    }
cleanup:
    bigdecimal_destroy(period);bigdecimal_destroy(offset);bigdecimal_destroy(target);bigdecimal_destroy(index);bigdecimal_destroy(delta);
    bigdecimal_destroy(ratio);bigdecimal_destroy(a);bigdecimal_destroy(b);bigdecimal_destroy(epsilon);bigdecimal_destroy(one);
    bigrational_destroy(lo);bigrational_destroy(hi);bigrational_destroy(fraction);bigrational_destroy(position);
#undef PEAK_TRY
#undef PEAK_RAT
    return status;
}

static CalculatorStatus adaptive_refine(GraphRefinement *work)
{
    typedef struct { size_t left;unsigned int depth; } Interval;
    Interval queue[8192U];
    size_t head = 0U, pending = 32U, max_bytes = 0U;
    uint64_t max_ms = 0U;
    for (size_t i = 0; i < pending; ++i) queue[i] = (Interval){i, 0U};
    while (pending != 0U) {
        ApplicationGraph *graph = work->graph;
        size_t bytes = numforge_budget_available_bytes();
        uint64_t ms = numforge_budget_available_ms();
        /* Leave room for projection/JSON and stop before a known next probe
         * would exhaust the shared budget. Never reset a caller's budget. */
        if (graph->count >= work->point_limit ||
            bytes <= 2U * 1024U * 1024U || max_bytes > (bytes - 2U * 1024U * 1024U) / 2U ||
            ms <= 200U || max_ms > (ms - 200U) / 2U) {
            graph->limited = true;break;
        }
        Interval current = queue[head];head = (head + 1U) % work->point_limit;--pending;
        if (current.depth >= 12U) { graph->limited = true;continue; }
        bool accepted, empty;
        CalculatorStatus status = adaptive_interval(work, current.left, &accepted, &empty);
        if (status != CALCULATOR_OK) return status;
        size_t after_bytes = numforge_budget_available_bytes();
        uint64_t after_ms = numforge_budget_available_ms();
        if (bytes >= after_bytes && bytes - after_bytes > max_bytes) max_bytes = bytes - after_bytes;
        if (ms >= after_ms && ms - after_ms > max_ms) max_ms = ms - after_ms;
        for (size_t i = 0; i < pending; ++i) {
            Interval *item = &queue[(head + i) % work->point_limit];
            if (item->left > current.left) ++item->left;
        }
        if (!accepted && !empty) {
            /* Active intervals are disjoint edges, so capacity follows the
             * point cap; two children replace their single parent. */
            queue[(head + pending++) % work->point_limit] = (Interval){current.left, current.depth + 1U};
            queue[(head + pending++) % work->point_limit] = (Interval){current.left + 1U, current.depth + 1U};
        }
    }
    return CALCULATOR_OK;
}

static CalculatorStatus adaptive_request(ApplicationGraph **result,
    const char *input, const char *parameter, const BigRational *minimum,
    const BigRational *maximum, size_t point_limit, const BigDecimal *y_minimum,
    const BigDecimal *y_maximum, size_t width, size_t height, const CalculatorSession *session,
    const CalculatorContext *context, CalculatorError *error)
{
    calculator_error_clear(error);
    if (result == NULL || input == NULL || minimum == NULL || maximum == NULL || context == NULL) {
        calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0U);return CALCULATOR_NULL_ARGUMENT;
    }
    bool camera = y_minimum != NULL || y_maximum != NULL;
    if (point_limit < 33U || point_limit > 8192U || (camera &&
        (y_minimum == NULL || y_maximum == NULL || width < 320U || width > 2048U || height < 160U || height > 1200U))) {
        calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, 0U);return CALCULATOR_INVALID_ARGUMENT;
    }
    CalculatorStatus status = calculator_context_validate(context, error);
    if (status != CALCULATOR_OK) return status;
    bool owner = numforge_budget_begin((uint64_t)context->time_limit_ms, CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    ApplicationGraph *graph = NULL;
    ApplicationExpression *expression = NULL;
    BigRational *step = NULL, *divisor = NULL, *cursor = NULL;
    BigDecimal *number = NULL, *factor = NULL;
    GraphRefinement work = {0};work.point_limit = point_limit;work.camera = camera;
    ApplicationExpressionSample *seeds = NULL;
    bool any = false;
    int comparison = 0;
    status = calculator_from_rational_status(bigrational_compare(&comparison, minimum, maximum));
    if (status == CALCULATOR_OK && comparison >= 0) status = CALCULATOR_INVALID_ARGUMENT;
    if (status == CALCULATOR_OK && camera) {
        status = calculator_from_decimal_status(bigdecimal_compare(&comparison, y_minimum, y_maximum));
        if (status == CALCULATOR_OK && comparison >= 0) status = CALCULATOR_INVALID_ARGUMENT;
    }
    if (status == CALCULATOR_OK) status = parameter == NULL ?
        application_expression_prepare_auto(&expression, input, session, context, error) :
        application_expression_prepare(&expression, input, parameter, session, context, error);
    if (status == CALCULATOR_OK) application_expression_enable_graph_phases(expression);
    work.smooth = application_expression_graph_smooth(expression);
    if (status == CALCULATOR_OK) {
        graph = numforge_calloc(1U, sizeof(*graph));
        if (graph == NULL) status = CALCULATOR_OUT_OF_MEMORY;
    }
    if (status == CALCULATOR_OK) {
        graph->count = 33U;
        memcpy(graph->parameter, application_expression_parameter(expression), strlen(application_expression_parameter(expression)) + 1U);
        graph->coordinates = numforge_calloc(point_limit, sizeof(*graph->coordinates));
        graph->samples = numforge_calloc(point_limit, sizeof(*graph->samples));
        graph->connect = numforge_calloc(point_limit, sizeof(*graph->connect));
        graph->view_minimum = bigdecimal_create();graph->view_maximum = bigdecimal_create();
        step = bigrational_create();divisor = bigrational_create();cursor = bigrational_create();
        number = bigdecimal_create();factor = bigdecimal_create();
        work.half = bigdecimal_create();work.sum = bigdecimal_create();work.delta = bigdecimal_create();work.tolerance = bigdecimal_create();work.phase_span = bigdecimal_create();work.coarse_width = bigdecimal_create();work.strict_phase_span = bigdecimal_create();
        if (camera) {
            work.clip_minimum = bigdecimal_create();work.clip_maximum = bigdecimal_create();
            work.camera_tolerance = bigdecimal_create();work.pixel_width = bigdecimal_create();
        }
        if (graph->coordinates == NULL || graph->samples == NULL || graph->connect == NULL || graph->view_minimum == NULL ||
            graph->view_maximum == NULL || step == NULL || divisor == NULL || cursor == NULL || number == NULL || factor == NULL ||
            work.half == NULL || work.sum == NULL || work.delta == NULL || work.tolerance == NULL || work.phase_span == NULL || work.coarse_width == NULL || work.strict_phase_span == NULL ||
            (camera && (work.clip_minimum == NULL || work.clip_maximum == NULL || work.camera_tolerance == NULL || work.pixel_width == NULL))) status = CALCULATOR_OUT_OF_MEMORY;
    }
    if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_set_string(number, "32"));
    if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_from_bigdecimal(divisor, number));
    if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_sub(step, maximum, minimum));
    if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_div(step, step, divisor));
    if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_copy(cursor, minimum));
    for (size_t i = 0; status == CALCULATOR_OK && i < 33U; ++i) {
        status = adaptive_coordinate(&graph->coordinates[i], cursor, context);
        if (status == CALCULATOR_OK && i < 32U) status = calculator_from_rational_status(bigrational_add(cursor, cursor, step));
    }
    if (status == CALCULATOR_OK) status = application_expression_sample(expression, graph->coordinates, 33U, &seeds, error);
    if (status == CALCULATOR_OK) {
        memcpy(graph->samples, seeds, 33U * sizeof(*seeds));free(seeds);seeds = NULL;
        status = adaptive_view(graph, &any);
    }
    if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_sub(work.tolerance, graph->view_maximum, graph->view_minimum));
    bool zero = false;
    if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_is_zero(&zero, work.tolerance));
    if (status == CALCULATOR_OK && zero) status = calculator_from_decimal_status(bigdecimal_abs(work.tolerance, graph->view_maximum));
    if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_is_zero(&zero, work.tolerance));
    if (status == CALCULATOR_OK && zero) status = calculator_from_decimal_status(bigdecimal_set_string(work.tolerance, "1"));
    if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_set_string(factor, work.smooth ? "0.005" : "0.001"));
    if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_mul(work.tolerance, work.tolerance, factor));
    if (status == CALCULATOR_OK && camera) {
        char pixels[24];(void)snprintf(pixels, sizeof(pixels), "%zu", height - 68U);
        /* A screen tolerance may tighten seed scaling, never relax a tiny seed
         * signal before refinement has had a chance to discover a narrow peak. */
        status = calculator_from_decimal_status(bigdecimal_copy(work.sum, work.tolerance));
        if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_sub(work.tolerance, y_maximum, y_minimum));
        if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_set_string(factor, pixels));
        if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_div_significant(work.tolerance, work.tolerance, factor, 40, BIGDECIMAL_ROUND_HALF_EVEN));
        if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_copy(work.camera_tolerance, work.tolerance));
        if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_add(work.delta, work.tolerance, work.tolerance));
        if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_sub(work.clip_minimum, y_minimum, work.delta));
        if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_add(work.clip_maximum, y_maximum, work.delta));
        if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_compare(&comparison, work.sum, work.tolerance));
        if (status == CALCULATOR_OK && comparison < 0) status = calculator_from_decimal_status(bigdecimal_copy(work.tolerance, work.sum));
    }
    if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_set_string(work.half, "0.5"));
    if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_set_string(work.strict_phase_span,
        context->angle_unit == CALCULATOR_ANGLE_DEGREES ? "30" : "0.5"));
    if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_set_string(work.phase_span,
        context->angle_unit == CALCULATOR_ANGLE_DEGREES ? (work.smooth ? "170" : "30") : (work.smooth ? "3" : "0.5")));
    if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_sub(work.coarse_width,
        graph->coordinates[32U].number,graph->coordinates[0U].number));
    if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_set_string(factor,"0.00390625"));
    if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_mul(work.coarse_width,work.coarse_width,factor));
    if (status == CALCULATOR_OK && camera) {
        char pixels[24];(void)snprintf(pixels, sizeof(pixels), "%zu", width - 86U);
        status = calculator_from_decimal_status(bigdecimal_sub(work.coarse_width, graph->coordinates[32U].number, graph->coordinates[0U].number));
        if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_set_string(factor, "8"));
        if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_mul(work.coarse_width, work.coarse_width, factor));
        if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_set_string(factor, pixels));
        if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_div_significant(work.coarse_width, work.coarse_width, factor, 40, BIGDECIMAL_ROUND_HALF_EVEN));
        if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_copy(work.pixel_width, work.coarse_width));
        for (size_t i = 0U; status == CALCULATOR_OK && i < 3U; ++i)
            status = calculator_from_decimal_status(bigdecimal_mul(work.pixel_width, work.pixel_width, work.half));
    }
    if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_set_string(number, "2"));
    if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_from_bigdecimal(divisor, number));
    work.graph = graph;work.expression = expression;work.context = context;work.error = error;work.two = divisor;
    if (status == CALCULATOR_OK) status = adaptive_refine(&work);
    if (status == CALCULATOR_OK) status = adaptive_peaks(&work);
    /* Include newly discovered extrema for resolved/smooth curves. Unresolved
     * pole/domain trees retain the seed overview instead of chasing infinity. */
    if (status == CALCULATOR_OK && (!any || !graph->limited || work.smooth)) status = adaptive_view(graph, &any);
    if (status == CALCULATOR_OK && !any) {
        bigdecimal_destroy(graph->view_minimum);graph->view_minimum = NULL;
        bigdecimal_destroy(graph->view_maximum);graph->view_maximum = NULL;
    }
    status = calculator_budget_status(status);
    application_expression_samples_destroy(seeds, 33U);application_expression_destroy(expression);
    bigrational_destroy(step);bigrational_destroy(divisor);bigrational_destroy(cursor);
    bigdecimal_destroy(number);bigdecimal_destroy(factor);bigdecimal_destroy(work.half);
    bigdecimal_destroy(work.sum);bigdecimal_destroy(work.delta);bigdecimal_destroy(work.tolerance);
    bigdecimal_destroy(work.phase_span);
    bigdecimal_destroy(work.coarse_width);
    bigdecimal_destroy(work.strict_phase_span);
    bigdecimal_destroy(work.clip_minimum);bigdecimal_destroy(work.clip_maximum);
    bigdecimal_destroy(work.camera_tolerance);bigdecimal_destroy(work.pixel_width);
    if (status == CALCULATOR_OK) *result = graph;
    else { application_graph_destroy(graph);calculator_error_set(error, status, error == NULL ? 0U : error->offset); }
    if (owner) numforge_budget_end();
    return status;
}

CalculatorStatus application_graph_adaptive_limit(ApplicationGraph **result,
    const char *input, const char *parameter, const BigRational *minimum,
    const BigRational *maximum, size_t point_limit, const CalculatorSession *session,
    const CalculatorContext *context, CalculatorError *error)
{
    return adaptive_request(result,input,parameter,minimum,maximum,point_limit,NULL,NULL,0U,0U,session,context,error);
}

CalculatorStatus application_graph_adaptive_camera(ApplicationGraph **result,
    const char *input, const char *parameter, const BigRational *minimum,
    const BigRational *maximum, const BigDecimal *y_minimum, const BigDecimal *y_maximum,
    size_t width, size_t height, const CalculatorSession *session,
    const CalculatorContext *context, CalculatorError *error)
{
    if (y_minimum == NULL || y_maximum == NULL) {
        calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0U);return CALCULATOR_NULL_ARGUMENT;
    }
    return adaptive_request(result,input,parameter,minimum,maximum,APPLICATION_GRAPH_MAX_ADAPTIVE_SAMPLES,
        y_minimum,y_maximum,width,height,session,context,error);
}

CalculatorStatus application_graph_adaptive(ApplicationGraph **result,
    const char *input, const char *parameter, const BigRational *minimum,
    const BigRational *maximum, const CalculatorSession *session,
    const CalculatorContext *context, CalculatorError *error)
{
    return application_graph_adaptive_limit(result,input,parameter,minimum,maximum,
        APPLICATION_GRAPH_MAX_ADAPTIVE_SAMPLES,session,context,error);
}
