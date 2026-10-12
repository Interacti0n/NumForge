#include "graph.h"
#include "exact_functions.h"
#include "value_internal.h"
#include <numforge/runtime.h>
#include <stdlib.h>
#include <stdio.h>

void application_graph_destroy(ApplicationGraph *graph)
{
    if (graph == NULL) return;
    if (graph->coordinates != NULL)
        for (size_t i = 0; i < graph->count; ++i) calculator_value_destroy(&graph->coordinates[i]);
    free(graph->coordinates);
    application_expression_samples_destroy(graph->samples, graph->count);
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
