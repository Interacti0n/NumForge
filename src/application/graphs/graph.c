#include "src/application/graphs/graph.h"
#include "src/application/numerical_status.h"
#include "src/engine/evaluator/exact_functions.h"
#include "src/engine/values/value_internal.h"
#include "src/runtime/numforge_alloc.h"
#include <numforge/runtime.h>
#include <numforge/numerical.h>
#include <numforge/curve.h>
#include <numforge/phase.h>
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

static CalculatorStatus graph_coordinate(CalculatorValue *value,
    const BigRational *rational, const CalculatorContext *context)
{
    value->kind = CALCULATOR_VALUE_RATIONAL;value->context = *context;value->independent = true;
    value->rational = bigrational_create();value->number = bigdecimal_create();
    if (value->rational == NULL || value->number == NULL) return CALCULATOR_OUT_OF_MEMORY;
    CalculatorStatus status = calculator_from_rational_status(bigrational_copy(value->rational, rational));
    if (status == CALCULATOR_OK) status = calculator_materialize_exact(value->number, value, context);
    return status;
}

typedef struct CoordinateGrid {
    CalculatorValue *coordinates;
    const CalculatorContext *context;
    CalculatorStatus status;
} CoordinateGrid;
static NumForgeNumericalStatus coordinate_visit(const BigRational *x, size_t index, void *user)
{
    CoordinateGrid *grid = user;
    grid->status = graph_coordinate(&grid->coordinates[index], x, grid->context);
    return grid->status == CALCULATOR_OK ? NUMFORGE_NUMERICAL_OK : NUMFORGE_NUMERICAL_CALLBACK_FAILED;
}
static CalculatorStatus graph_coordinates(CalculatorValue *coordinates, size_t count,
    const BigRational *minimum, const BigRational *maximum, const CalculatorContext *context)
{
    CoordinateGrid grid = {coordinates, context, CALCULATOR_OK};
    NumForgeNumericalStatus status = numforge_grid_visit(minimum, maximum, count, coordinate_visit, &grid);
    return application_from_numerical_status(status, grid.status);
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
    int comparison = 0;
    status = calculator_from_rational_status(bigrational_compare(&comparison, minimum, maximum));
    if (status == CALCULATOR_OK && comparison >= 0) status = CALCULATOR_INVALID_ARGUMENT;
    if (status == CALCULATOR_OK)
        status = application_expression_prepare(&expression, input, parameter, session, context, error);
    if (status == CALCULATOR_OK)
    {
        graph = numforge_calloc(1U, sizeof(*graph));
        if (graph == NULL)
            status = CALCULATOR_OUT_OF_MEMORY;
    }
    if (status == CALCULATOR_OK)
    {
        graph->count = count;
        graph->coordinates = numforge_calloc(count, sizeof(*graph->coordinates));
        if (graph->coordinates == NULL) status = CALCULATOR_OUT_OF_MEMORY;
    }
    if (status == CALCULATOR_OK)
        status = graph_coordinates(graph->coordinates, count, minimum, maximum, context);
    if (status == CALCULATOR_OK)
        status = application_expression_sample(expression, graph->coordinates, count, &graph->samples, error);
    status = calculator_budget_status(status);
    application_expression_destroy(expression);
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
    NumForgeCurveClassifier *classifier;
    bool smooth;
    size_t point_limit;
    CalculatorStatus callback_status;
} GraphRefinement;

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
    if (status == CALCULATOR_OK) status = graph_coordinate(&midpoint, middle, work->context);
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
    NumForgeCurveSample samples[3] = {0};
    const BigDecimal *channels[3][APPLICATION_EXPRESSION_MAX_PHASES];
    for (size_t i = 0; i < 3; ++i) {
        const ApplicationExpressionSample *sample = &graph->samples[left + i];
        samples[i].x = graph->coordinates[left + i].number; samples[i].y = sample->value.number;
        samples[i].real = application_graph_point_kind(sample) == APPLICATION_GRAPH_REAL;
        samples[i].channel_count = sample->phase_count; samples[i].channels_unknown = sample->phase_unknown;
        samples[i].channels = channels[i];
        for (size_t j = 0; j < sample->phase_count; ++j) channels[i][j] = sample->phases[j];
    }
    NumForgeCurveAssessment assessment;
    status = application_from_numerical_status(numforge_curve_assess(&assessment, work->classifier, samples), CALCULATOR_OK);
    if (status == CALCULATOR_OK) {
        *accepted = assessment.decision == NUMFORGE_REFINEMENT_ACCEPT;
        *empty = assessment.decision == NUMFORGE_REFINEMENT_SKIP;
        graph->connect[left + 1] = graph->connect[left + 2] = *accepted;
        graph->screen_clipped = graph->screen_clipped || assessment.screen_clipped;
    }
    return status;
}

typedef struct PhaseProbe {
    GraphRefinement *work;
    size_t phase;
    CalculatorValue coordinate;
    ApplicationExpressionSample *sample;
    bool point_limited;
} PhaseProbe;
static void phase_probe_clear(PhaseProbe *probe)
{
    calculator_value_destroy(&probe->coordinate);
    application_expression_samples_destroy(probe->sample, 1); probe->sample = NULL;
}
static NumForgeNumericalStatus phase_evaluate(BigDecimal *value, bool *usable,
    const BigRational *x, void *user)
{
    PhaseProbe *probe = user;
    GraphRefinement *work = probe->work;
    phase_probe_clear(probe);
    if (work->graph->count >= work->point_limit) { probe->point_limited = true; return NUMFORGE_NUMERICAL_OK; }
    work->callback_status = graph_coordinate(&probe->coordinate, x, work->context);
    if (work->callback_status == CALCULATOR_OK)
        work->callback_status = application_expression_sample(work->expression, &probe->coordinate, 1, &probe->sample, work->error);
    if (work->callback_status != CALCULATOR_OK) return NUMFORGE_NUMERICAL_CALLBACK_FAILED;
    *usable = application_graph_point_kind(probe->sample) == APPLICATION_GRAPH_REAL && !probe->sample->phase_unknown &&
        probe->phase < probe->sample->phase_count && probe->sample->phases[probe->phase] != NULL;
    if (*usable) work->callback_status = calculator_from_decimal_status(bigdecimal_copy(value, probe->sample->phases[probe->phase]));
    return work->callback_status == CALCULATOR_OK ? NUMFORGE_NUMERICAL_OK : NUMFORGE_NUMERICAL_CALLBACK_FAILED;
}
/* Locate phase anchors through the numeric kernel; retain the callback's actual
 * typed original-function sample when the captured phase passes its tolerance. */
static CalculatorStatus adaptive_peaks(GraphRefinement *work)
{
    ApplicationGraph *graph = work->graph;
    if (!work->smooth || !graph->samples[0].phase_count) return CALCULATOR_OK;
    BigDecimal *period = bigdecimal_create(), *offset = bigdecimal_create();
    NumForgePhaseLocator *locator = NULL;
    PhaseProbe probe = {0}; probe.work = work;
    CalculatorStatus status = period && offset ? CALCULATOR_OK : CALCULATOR_OUT_OF_MEMORY;
    int64_t digits = work->context->division_scale < 34 ? 34 : work->context->division_scale;
    if (status == CALCULATOR_OK) status = calculator_from_decimal_status(work->context->angle_unit == CALCULATOR_ANGLE_DEGREES ?
        bigdecimal_set_string(period, "180") : bigdecimal_set_constant_significant(period, BIGDECIMAL_CONSTANT_PI, digits + 16, BIGDECIMAL_ROUND_HALF_EVEN));
    if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_mul(offset, period, work->half));
    if (status == CALCULATOR_OK) {
        NumForgePhaseOptions options; numforge_phase_options_init(&options);
        options.period = period; options.significant_digits = digits;
        options.reserve_bytes = 3U * 1024U * 1024U; options.reserve_ms = 300;
        status = application_from_numerical_status(numforge_phase_locator_create(&locator, &options), CALCULATOR_OK);
    }
    for (size_t edge = 1; status == CALCULATOR_OK && edge < graph->count; ++edge) {
        if (!graph->connect[edge]) continue;
        for (size_t phase = 0; status == CALCULATOR_OK && phase < graph->samples[edge - 1].phase_count; ++phase) {
            int kind = application_expression_graph_peak_offset(work->expression, phase);
            if (kind < 0 || graph->samples[edge - 1].phase_unknown || graph->samples[edge].phase_unknown) continue;
            const BigDecimal *p = graph->samples[edge - 1].phases[phase], *q = graph->samples[edge].phases[phase];
            if (!p || !q) continue;
            probe.phase = phase; work->callback_status = CALCULATOR_OK;
            NumForgePhaseReport report;
            NumForgeNumericalStatus found = numforge_phase_locate(&report, locator,
                graph->coordinates[edge - 1].rational, graph->coordinates[edge].rational,
                p, q, kind ? offset : NULL, phase_evaluate, &probe);
            status = application_from_numerical_status(found, work->callback_status);
            if (status != CALCULATOR_OK) break;
            if (probe.point_limited || report.kind == NUMFORGE_PHASE_RESOURCE_LIMIT) { graph->limited = true; goto cleanup; }
            if (report.kind == NUMFORGE_PHASE_FOUND) {
                size_t tail = graph->count - edge;
                memmove(graph->coordinates + edge + 1, graph->coordinates + edge, tail * sizeof(*graph->coordinates));
                memmove(graph->samples + edge + 1, graph->samples + edge, tail * sizeof(*graph->samples));
                memmove(graph->connect + edge + 1, graph->connect + edge, tail * sizeof(*graph->connect));
                graph->coordinates[edge] = probe.coordinate; probe.coordinate = (CalculatorValue){0};
                graph->samples[edge] = probe.sample[0]; free(probe.sample); probe.sample = NULL;
                graph->connect[edge] = true; ++graph->count; ++edge;
            }
            phase_probe_clear(&probe);
        }
    }
cleanup:
    phase_probe_clear(&probe); numforge_phase_locator_destroy(locator);
    bigdecimal_destroy(period); bigdecimal_destroy(offset); return status;
}

static NumForgeNumericalStatus adaptive_probe_callback(NumForgeRefinementDecision *decision,
    size_t left, size_t depth, void *user)
{
    GraphRefinement *work = user;
    bool accepted = false, empty = false;
    (void)depth;
    work->callback_status = adaptive_interval(work, left, &accepted, &empty);
    if (work->callback_status != CALCULATOR_OK) return NUMFORGE_NUMERICAL_CALLBACK_FAILED;
    *decision = accepted ? NUMFORGE_REFINEMENT_ACCEPT : empty ? NUMFORGE_REFINEMENT_SKIP : NUMFORGE_REFINEMENT_SPLIT;
    return NUMFORGE_NUMERICAL_OK;
}
static CalculatorStatus adaptive_refine(GraphRefinement *work)
{
    NumForgeRefinementOptions options;
    numforge_refinement_options_init(&options);
    options.max_points = work->point_limit;
    /* Retain graph projection/JSON headroom under the existing shared budget. */
    options.reserve_bytes = 2U * 1024U * 1024U; options.reserve_ms = 200;
    NumForgeRefinementStats stats;
    NumForgeNumericalStatus status = numforge_refine_intervals(&stats, work->graph->count,
        &options, adaptive_probe_callback, work);
    if (status == NUMFORGE_NUMERICAL_OK && stats.limits) work->graph->limited = true;
    return application_from_numerical_status(status, work->callback_status);
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
    BigRational *divisor = NULL;
    BigDecimal *number = NULL, *factor = NULL;
    GraphRefinement work = {0};work.point_limit = point_limit;
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
        divisor = bigrational_create();
        number = bigdecimal_create();factor = bigdecimal_create();
        work.half = bigdecimal_create();work.sum = bigdecimal_create();work.delta = bigdecimal_create();work.tolerance = bigdecimal_create();work.phase_span = bigdecimal_create();work.coarse_width = bigdecimal_create();work.strict_phase_span = bigdecimal_create();
        if (camera) {
            work.clip_minimum = bigdecimal_create();work.clip_maximum = bigdecimal_create();
            work.camera_tolerance = bigdecimal_create();work.pixel_width = bigdecimal_create();
        }
        if (graph->coordinates == NULL || graph->samples == NULL || graph->connect == NULL || graph->view_minimum == NULL ||
            graph->view_maximum == NULL || divisor == NULL || number == NULL || factor == NULL ||
            work.half == NULL || work.sum == NULL || work.delta == NULL || work.tolerance == NULL || work.phase_span == NULL || work.coarse_width == NULL || work.strict_phase_span == NULL ||
            (camera && (work.clip_minimum == NULL || work.clip_maximum == NULL || work.camera_tolerance == NULL || work.pixel_width == NULL))) status = CALCULATOR_OUT_OF_MEMORY;
    }
    if (status == CALCULATOR_OK) status = graph_coordinates(graph->coordinates, 33U, minimum, maximum, context);
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
    if (status == CALCULATOR_OK) {
        NumForgeCurveChannelKind channels[APPLICATION_EXPRESSION_MAX_PHASES];
        size_t channel_count = application_expression_graph_phase_count(expression);
        for (size_t i = 0; i < channel_count; ++i) channels[i] =
            application_expression_graph_peak_offset(expression, i) == -2 ? NUMFORGE_CURVE_STEP : NUMFORGE_CURVE_PHASE;
        NumForgeCurveCamera view = {work.clip_minimum, work.clip_maximum, work.camera_tolerance, work.pixel_width};
        NumForgeCurveOptions options = {work.tolerance, work.phase_span, work.strict_phase_span,
            work.coarse_width, work.smooth, camera ? &view : NULL, channels, channel_count};
        status = application_from_numerical_status(numforge_curve_classifier_create(&work.classifier, &options), CALCULATOR_OK);
    }
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
    numforge_curve_classifier_destroy(work.classifier);
    bigrational_destroy(divisor);
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
