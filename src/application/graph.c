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
 * Seeded y bounds keep newly discovered poles from crushing the visible scale. */
typedef struct GraphRefinement {
    ApplicationGraph *graph;
    ApplicationExpression *expression;
    const CalculatorContext *context;
    CalculatorError *error;
    BigRational *two;
    BigDecimal *half, *sum, *delta, *tolerance;
    BigDecimal *phase_span;
} GraphRefinement;

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
        if (status != CALCULATOR_OK) return status;
        bool phases_safe = true;
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
                }
        if (comparison <= 0 && phases_safe) {
            graph->connect[left + 1U] = true;graph->connect[left + 2U] = true;*accepted = true;
        }
    }
    return status;
}

static CalculatorStatus adaptive_refine(GraphRefinement *work)
{
    typedef struct { size_t left;unsigned int depth; } Interval;
    Interval queue[APPLICATION_EXPRESSION_MAX_SAMPLES];
    size_t head = 0U, pending = 32U, max_bytes = 0U;
    uint64_t max_ms = 0U;
    for (size_t i = 0; i < pending; ++i) queue[i] = (Interval){i, 0U};
    while (pending != 0U) {
        ApplicationGraph *graph = work->graph;
        size_t bytes = numforge_budget_available_bytes();
        uint64_t ms = numforge_budget_available_ms();
        /* Leave room for projection/JSON and stop before a known next probe
         * would exhaust the shared budget. Never reset a caller's budget. */
        if (graph->count >= APPLICATION_EXPRESSION_MAX_SAMPLES ||
            bytes <= 2U * 1024U * 1024U || max_bytes > (bytes - 2U * 1024U * 1024U) / 2U ||
            ms <= 200U || max_ms > (ms - 200U) / 2U) {
            graph->limited = true;break;
        }
        Interval current = queue[head];head = (head + 1U) % APPLICATION_EXPRESSION_MAX_SAMPLES;--pending;
        if (current.depth >= 12U) { graph->limited = true;continue; }
        bool accepted, empty;
        CalculatorStatus status = adaptive_interval(work, current.left, &accepted, &empty);
        if (status != CALCULATOR_OK) return status;
        size_t after_bytes = numforge_budget_available_bytes();
        uint64_t after_ms = numforge_budget_available_ms();
        if (bytes >= after_bytes && bytes - after_bytes > max_bytes) max_bytes = bytes - after_bytes;
        if (ms >= after_ms && ms - after_ms > max_ms) max_ms = ms - after_ms;
        for (size_t i = 0; i < pending; ++i) {
            Interval *item = &queue[(head + i) % APPLICATION_EXPRESSION_MAX_SAMPLES];
            if (item->left > current.left) ++item->left;
        }
        if (!accepted && !empty) {
            /* Active intervals are disjoint edges, so capacity follows the
             * point cap; two children replace their single parent. */
            queue[(head + pending++) % APPLICATION_EXPRESSION_MAX_SAMPLES] = (Interval){current.left, current.depth + 1U};
            queue[(head + pending++) % APPLICATION_EXPRESSION_MAX_SAMPLES] = (Interval){current.left + 1U, current.depth + 1U};
        }
    }
    return CALCULATOR_OK;
}

CalculatorStatus application_graph_adaptive(ApplicationGraph **result,
    const char *input, const char *parameter, const BigRational *minimum,
    const BigRational *maximum, const CalculatorSession *session,
    const CalculatorContext *context, CalculatorError *error)
{
    calculator_error_clear(error);
    if (result == NULL || input == NULL || minimum == NULL || maximum == NULL || context == NULL) {
        calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0U);return CALCULATOR_NULL_ARGUMENT;
    }
    CalculatorStatus status = calculator_context_validate(context, error);
    if (status != CALCULATOR_OK) return status;
    bool owner = numforge_budget_begin((uint64_t)context->time_limit_ms, CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    ApplicationGraph *graph = NULL;
    ApplicationExpression *expression = NULL;
    BigRational *step = NULL, *divisor = NULL, *cursor = NULL;
    BigDecimal *number = NULL, *factor = NULL;
    GraphRefinement work = {0};
    ApplicationExpressionSample *seeds = NULL;
    bool any = false;
    int comparison = 0;
    status = calculator_from_rational_status(bigrational_compare(&comparison, minimum, maximum));
    if (status == CALCULATOR_OK && comparison >= 0) status = CALCULATOR_INVALID_ARGUMENT;
    if (status == CALCULATOR_OK) status = parameter == NULL ?
        application_expression_prepare_auto(&expression, input, session, context, error) :
        application_expression_prepare(&expression, input, parameter, session, context, error);
    if (status == CALCULATOR_OK) application_expression_enable_graph_phases(expression);
    if (status == CALCULATOR_OK) {
        graph = numforge_calloc(1U, sizeof(*graph));
        if (graph == NULL) status = CALCULATOR_OUT_OF_MEMORY;
    }
    if (status == CALCULATOR_OK) {
        graph->count = 33U;
        memcpy(graph->parameter, application_expression_parameter(expression), strlen(application_expression_parameter(expression)) + 1U);
        graph->coordinates = numforge_calloc(APPLICATION_EXPRESSION_MAX_SAMPLES, sizeof(*graph->coordinates));
        graph->samples = numforge_calloc(APPLICATION_EXPRESSION_MAX_SAMPLES, sizeof(*graph->samples));
        graph->connect = numforge_calloc(APPLICATION_EXPRESSION_MAX_SAMPLES, sizeof(*graph->connect));
        graph->view_minimum = bigdecimal_create();graph->view_maximum = bigdecimal_create();
        step = bigrational_create();divisor = bigrational_create();cursor = bigrational_create();
        number = bigdecimal_create();factor = bigdecimal_create();
        work.half = bigdecimal_create();work.sum = bigdecimal_create();work.delta = bigdecimal_create();work.tolerance = bigdecimal_create();work.phase_span = bigdecimal_create();
        if (graph->coordinates == NULL || graph->samples == NULL || graph->connect == NULL || graph->view_minimum == NULL ||
            graph->view_maximum == NULL || step == NULL || divisor == NULL || cursor == NULL || number == NULL || factor == NULL ||
            work.half == NULL || work.sum == NULL || work.delta == NULL || work.tolerance == NULL || work.phase_span == NULL) status = CALCULATOR_OUT_OF_MEMORY;
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
    if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_set_string(factor, "0.001"));
    if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_mul(work.tolerance, work.tolerance, factor));
    if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_set_string(work.half, "0.5"));
    if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_set_string(work.phase_span,
        context->angle_unit == CALCULATOR_ANGLE_DEGREES ? "30" : "0.5"));
    if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_set_string(number, "2"));
    if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_from_bigdecimal(divisor, number));
    work.graph = graph;work.expression = expression;work.context = context;work.error = error;work.two = divisor;
    if (status == CALCULATOR_OK) status = adaptive_refine(&work);
    if (status == CALCULATOR_OK && !any) status = adaptive_view(graph, &any);
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
    if (status == CALCULATOR_OK) *result = graph;
    else { application_graph_destroy(graph);calculator_error_set(error, status, error == NULL ? 0U : error->offset); }
    if (owner) numforge_budget_end();
    return status;
}
