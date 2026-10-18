#include "numeric_root.h"
#include "numeric_root_brent_internal.h"
#include "expression.h"
#include "exact_functions.h"
#include "../internal/numforge_alloc.h"
#include <stdlib.h>
#include <string.h>

typedef struct Probe { BigDecimal *value; int sign; bool real, exact_zero; } Probe;
typedef struct Work { ApplicationExpression *sides[2]; const char *sources[2]; const BigDecimal *initial; CalculatorContext context; size_t evaluations; size_t rhs; } Work;

const char *application_numeric_root_kind(ApplicationNumericRootKind kind)
{
    switch (kind) {
        case APPLICATION_ROOT_FOUND: return "found";
        case APPLICATION_ROOT_NOT_BRACKETED: return "not_bracketed";
        case APPLICATION_ROOT_DOMAIN_GAP: return "domain_gap";
        case APPLICATION_ROOT_DOMAIN_UNVERIFIED: return "domain_unverified";
        case APPLICATION_ROOT_ITERATION_LIMIT: return "iteration_limit";
        case APPLICATION_ROOT_PRECISION_LIMIT: return "precision_limit";
        default: return "iteration_limit";
    }
}
void application_numeric_root_destroy(ApplicationNumericRoot *result)
{
    if (!result) return;
    bigrational_destroy(result->x); bigrational_destroy(result->lower); bigrational_destroy(result->upper);
    bigdecimal_destroy(result->residual); free(result);
}
static CalculatorStatus rational(BigRational *result, const CalculatorValue *value)
{
    return calculator_from_rational_status(value->kind == CALCULATOR_VALUE_INTEGER
        ? bigrational_from_bigint(result, value->integer) : bigrational_copy(result, value->rational));
}
static bool exact(const CalculatorValue *value)
{
    return value->kind == CALCULATOR_VALUE_INTEGER || value->kind == CALCULATOR_VALUE_RATIONAL;
}
static CalculatorStatus probe(Work *work, const BigRational *x, Probe *result, CalculatorError *error)
{
    result->real = false; result->exact_zero = false; result->sign = 0;
    CalculatorValue argument = {0}; ApplicationExpressionSample *samples[2] = {0};
    BigRational *values[2] = {0};
    argument.kind = CALCULATOR_VALUE_RATIONAL; argument.context = work->context;
    argument.independent = true; argument.rational = bigrational_create(); argument.number = bigdecimal_create();
    CalculatorStatus status = argument.rational && argument.number ? CALCULATOR_OK : CALCULATOR_OUT_OF_MEMORY;
    if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_copy(argument.rational, x));
    if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_to_bigdecimal(argument.number, x, work->context.division_scale, BIGDECIMAL_ROUND_HALF_EVEN));
    for (size_t i = 0U; i < 2U && status == CALCULATOR_OK; ++i) {
        status = application_expression_sample(work->sides[i], &argument, 1U, &samples[i], error);
        if (status != CALCULATOR_OK && i) calculator_error_set(error, status, work->rhs + (error ? error->offset : 0U));
    }
    if (status == CALCULATOR_OK) {
        ++work->evaluations;
        result->real = samples[0]->error.status == CALCULATOR_OK && samples[1]->error.status == CALCULATOR_OK &&
            !samples[0]->value.quantity && !samples[1]->value.quantity &&
            !calculator_value_is_complex(&samples[0]->value) && !calculator_value_is_complex(&samples[1]->value);
        if (result->real && exact(&samples[0]->value) && exact(&samples[1]->value)) {
            values[0] = bigrational_create(); values[1] = bigrational_create();
            if (!values[0] || !values[1]) status = CALCULATOR_OUT_OF_MEMORY;
            for (size_t i = 0U; i < 2U && status == CALCULATOR_OK; ++i) status = rational(values[i], &samples[i]->value);
            if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_sub(values[0], values[0], values[1]));
            if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_to_bigdecimal(result->value, values[0], work->context.division_scale, BIGDECIMAL_ROUND_HALF_EVEN));
            if (status == CALCULATOR_OK) {
                int comparison = 0;
                /* values[1] is no longer needed; use an exact zero comparison. */
                BigRational *zero = bigrational_create();
                if (!zero) status = CALCULATOR_OUT_OF_MEMORY;
                else status = calculator_from_rational_status(bigrational_compare(&comparison, values[0], zero));
                result->exact_zero = comparison == 0;
                bigrational_destroy(zero);
            }
        } else if (result->real)
            status = calculator_from_decimal_status(bigdecimal_sub(result->value, samples[0]->value.number, samples[1]->value.number));
        if (status == CALCULATOR_OK && result->real) status = calculator_from_decimal_status(bigdecimal_sign(&result->sign, result->value));
    }
    for (size_t i = 0U; i < 2U; ++i) { application_expression_samples_destroy(samples[i], 1U); bigrational_destroy(values[i]); }
    calculator_value_destroy(&argument); return status;
}
static CalculatorStatus midpoint(BigRational *result, const BigRational *a, const BigRational *b, const BigRational *two)
{
    CalculatorStatus status = calculator_from_rational_status(bigrational_add(result, a, b));
    if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_div(result, result, two));
    return status;
}
static CalculatorStatus found(Work *work, ApplicationNumericRoot *result, const BigRational *x, const Probe *sample)
{
    if (!sample->exact_zero) for (size_t i = 0U; i < 2U; ++i) {
        bool verified = false;
        CalculatorStatus status = application_expression_domain(&verified, work->sides[i], result->lower, result->upper);
        if (status != CALCULATOR_OK) return status;
        if (!verified) { result->kind = APPLICATION_ROOT_DOMAIN_UNVERIFIED; return CALCULATOR_OK; }
    }
    /* A small/zero residual at the search precision can hide subtraction of
     * large rounded values. Reevaluate the original expressions at twice the
     * working precision; never substitute a transformed/interpolated formula.
     * This is a stability check, not certified forward-error arithmetic. */
    Work verification = {0}; Probe checked = {0};
    CalculatorStatus status = CALCULATOR_OK;
    if (!sample->exact_zero) {
        if (work->evaluations >= 192U) { result->kind = APPLICATION_ROOT_ITERATION_LIMIT; return CALCULATOR_OK; }
        verification.context = work->context; verification.context.division_scale *= 2;
        verification.rhs = work->rhs; checked.value = bigdecimal_create();
        if (!checked.value) status = CALCULATOR_OUT_OF_MEMORY;
        for (size_t i = 0U; i < 2U && status == CALCULATOR_OK; ++i) {
            CalculatorError ignored;
            status = application_expression_prepare(&verification.sides[i], work->sources[i], "x", NULL, &verification.context, &ignored);
        }
        if (status == CALCULATOR_OK) status = probe(&verification, x, &checked, NULL);
        work->evaluations += verification.evaluations;
        if (status == CALCULATOR_OK && !checked.real) result->kind = APPLICATION_ROOT_DOMAIN_GAP;
        if (status == CALCULATOR_OK && checked.real) {
            BigDecimal *tolerance = bigdecimal_create(); int order = 1;
            if (!tolerance) status = CALCULATOR_OUT_OF_MEMORY;
            if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_set_string(tolerance, "1E-12"));
            if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_abs(checked.value, checked.value));
            if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_compare(&order, checked.value, tolerance));
            if (status == CALCULATOR_OK && (order > 0 || (sample->sign && checked.sign != sample->sign))) result->kind = APPLICATION_ROOT_PRECISION_LIMIT;
            if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_compare(&order, checked.value, work->initial));
            if (status == CALCULATOR_OK && order >= 0) result->kind = APPLICATION_ROOT_PRECISION_LIMIT;
            bigdecimal_destroy(tolerance);
        }
        for (size_t i = 0U; i < 2U; ++i) application_expression_destroy(verification.sides[i]);
        if (status != CALCULATOR_OK || result->kind != APPLICATION_ROOT_ITERATION_LIMIT) {
            bigdecimal_destroy(checked.value); return status;
        }
    }
    result->x = bigrational_create(); result->residual = bigdecimal_create();
    status = result->x && result->residual ? CALCULATOR_OK : CALCULATOR_OUT_OF_MEMORY;
    if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_copy(result->x, x));
    if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_abs(result->residual, checked.value ? checked.value : sample->value));
    if (status == CALCULATOR_OK) { result->kind = APPLICATION_ROOT_FOUND; result->exact_zero = sample->exact_zero; }
    bigdecimal_destroy(checked.value); return status;
}

static CalculatorStatus solve(ApplicationNumericRoot **result, const char *input,
    const BigRational *lower, const BigRational *upper, const CalculatorContext *context, CalculatorError *error,
    bool brent, ApplicationNumericRootStats *statistics)
{
    calculator_error_clear(error);
    if (!result || !input || !lower || !upper || !context) { calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0U); return CALCULATOR_NULL_ARGUMENT; }
    CalculatorStatus status = calculator_context_validate(context, error);
    if (status != CALCULATOR_OK) return status;
    if (context->division_scale < 16 || context->division_scale > 100 || context->angle_unit != CALCULATOR_ANGLE_RADIANS) {
        calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, 0U); return CALCULATOR_INVALID_ARGUMENT;
    }
    size_t length = 0U;
    while (length <= CALCULATOR_MAX_INPUT_BYTES && input[length]) ++length;
    if (length > CALCULATOR_MAX_INPUT_BYTES) { calculator_error_set(error, CALCULATOR_VALUE_TOO_LARGE, 0U); return CALCULATOR_VALUE_TOO_LARGE; }
    const char *equal = strchr(input, '=');
    if (!equal || strchr(equal+1, '=') || strchr(input, '\n') || strchr(input, '\r')) {
        calculator_error_set(error, CALCULATOR_SYNTAX_ERROR, equal ? (size_t)(equal-input) : length); return CALCULATOR_SYNTAX_ERROR;
    }
    bool owner = numforge_budget_begin((uint64_t)context->time_limit_ms, CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    Work work = {0}; work.context = *context; work.context.division_scale += 16; work.context.significant_division = true;
    work.rhs = (size_t)(equal-input)+1U;
    ApplicationNumericRoot *output = numforge_calloc(1U, sizeof(*output));
    char *copy = numforge_malloc(length+1U);
    enum { MID, LEFT_QUARTER, RIGHT_QUARTER, TWO, WIDTH, TOLERANCE, SCRATCH_COUNT };
    BigRational *scratch[SCRATCH_COUNT] = {0};
    Probe probes[3] = {{0}}; BigDecimal *absolute = NULL, *tolerance = NULL, *initial = NULL;
    int order = 0, left_sign = 0, right_sign = 0;
    NumericRootBrent interpolation = {0}; ApplicationNumericRootStats stats = {0};
    if (!output || !copy) status = CALCULATOR_OUT_OF_MEMORY;
    if (status == CALCULATOR_OK) {
        memcpy(copy, input, length+1U); copy[work.rhs-1U] = '\0';
        work.sources[0] = copy; work.sources[1] = copy+work.rhs;
        output->kind = APPLICATION_ROOT_ITERATION_LIMIT;
        output->lower = bigrational_create(); output->upper = bigrational_create();
        absolute = bigdecimal_create(); tolerance = bigdecimal_create(); initial = bigdecimal_create();
        work.initial = initial;
        if (!output->lower || !output->upper || !absolute || !tolerance || !initial) status = CALCULATOR_OUT_OF_MEMORY;
    }
    for (size_t i = 0U; i < SCRATCH_COUNT && status == CALCULATOR_OK; ++i) {
        scratch[i] = bigrational_create(); if (!scratch[i]) status = CALCULATOR_OUT_OF_MEMORY;
    }
    for (size_t i = 0U; i < 3U && status == CALCULATOR_OK; ++i) {
        probes[i].value = bigdecimal_create(); if (!probes[i].value) status = CALCULATOR_OUT_OF_MEMORY;
    }
    if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_compare(&order, lower, upper));
    if (status == CALCULATOR_OK && order >= 0) status = CALCULATOR_INVALID_ARGUMENT;
    if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_copy(output->lower, lower));
    if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_copy(output->upper, upper));
    if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_set_string(tolerance, "1E-12"));
    if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_from_bigdecimal(scratch[TOLERANCE], tolerance));
    if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_set_string(absolute, "2"));
    if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_from_bigdecimal(scratch[TWO], absolute));
    for (size_t i = 0U; i < 2U && status == CALCULATOR_OK; ++i) {
        status = application_expression_prepare(&work.sides[i], copy+(i ? work.rhs : 0U), "x", NULL, &work.context, error);
        if (status != CALCULATOR_OK && i) calculator_error_set(error, status, work.rhs+(error ? error->offset : 0U));
    }
    for (size_t i = 0U; i < 2U && status == CALCULATOR_OK; ++i) {
        status = probe(&work, i ? upper : lower, &probes[i], error);
        if (status == CALCULATOR_OK && !probes[i].real) { output->kind = APPLICATION_ROOT_DOMAIN_GAP; break; }
        if (status == CALCULATOR_OK && probes[i].exact_zero) { status = found(&work, output, i ? upper : lower, &probes[i]); break; }
        if (!i) left_sign = probes[i].sign; else right_sign = probes[i].sign;
    }
    if (status == CALCULATOR_OK && output->kind == APPLICATION_ROOT_ITERATION_LIMIT &&
        (!left_sign || !right_sign || left_sign == right_sign)) output->kind = APPLICATION_ROOT_NOT_BRACKETED;
    if (status == CALCULATOR_OK && output->kind == APPLICATION_ROOT_ITERATION_LIMIT) {
        status = calculator_from_decimal_status(bigdecimal_abs(initial, probes[0].value));
        if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_abs(absolute, probes[1].value));
        if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_compare(&order, absolute, initial));
        if (status == CALCULATOR_OK && order < 0) status = calculator_from_decimal_status(bigdecimal_copy(initial, absolute));
        if (status == CALCULATOR_OK && brent) status = numeric_root_brent_create(&interpolation, work.context.division_scale);
        if (status == CALCULATOR_OK && brent) status = numeric_root_brent_reset(&interpolation, lower, upper, probes[0].value, probes[1].value);
    }
    for (size_t iteration = 0U; iteration < 96U && status == CALCULATOR_OK &&
        output->kind == APPLICATION_ROOT_ITERATION_LIMIT && work.evaluations < 189U; ++iteration) {
        ++output->iterations;
        if (brent) status = numeric_root_brent_propose(&interpolation, scratch[MID], &stats);
        else { status = midpoint(scratch[MID], output->lower, output->upper, scratch[TWO]); ++stats.bisections; }
        if (status == CALCULATOR_OK) status = probe(&work, scratch[MID], &probes[0], error);
        if (status != CALCULATOR_OK) break;
        if (!probes[0].real) { output->kind = APPLICATION_ROOT_DOMAIN_GAP; break; }
        if (probes[0].exact_zero) { status = found(&work, output, scratch[MID], &probes[0]); break; }
        status = calculator_from_rational_status(bigrational_sub(scratch[WIDTH], output->upper, output->lower));
        if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_compare(&order, scratch[WIDTH], scratch[TOLERANCE]));
        int residual_order = 1, improvement = 1;
        if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_abs(absolute, probes[0].value));
        if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_compare(&residual_order, absolute, tolerance));
        if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_compare(&improvement, absolute, initial));
        if (status != CALCULATOR_OK) break;
        if (!probes[0].sign) {
            status = midpoint(scratch[LEFT_QUARTER], output->lower, scratch[MID], scratch[TWO]);
            if (status == CALCULATOR_OK) status = midpoint(scratch[RIGHT_QUARTER], scratch[MID], output->upper, scratch[TWO]);
            if (status == CALCULATOR_OK) status = probe(&work, scratch[LEFT_QUARTER], &probes[1], error);
            if (status == CALCULATOR_OK) status = probe(&work, scratch[RIGHT_QUARTER], &probes[2], error);
            if (status != CALCULATOR_OK) break;
            if (!probes[1].real || !probes[2].real) { output->kind = APPLICATION_ROOT_DOMAIN_GAP; break; }
            if (!probes[1].sign || !probes[2].sign || probes[1].sign == probes[2].sign) { output->kind = APPLICATION_ROOT_PRECISION_LIMIT; break; }
            if (order <= 0 && residual_order <= 0 && improvement < 0) { status = found(&work, output, scratch[MID], &probes[0]); break; }
            left_sign = probes[1].sign; right_sign = probes[2].sign;
            status = calculator_from_rational_status(bigrational_copy(output->lower, scratch[LEFT_QUARTER]));
            if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_copy(output->upper, scratch[RIGHT_QUARTER]));
            if (status == CALCULATOR_OK && brent) status = numeric_root_brent_reset(&interpolation, output->lower, output->upper, probes[1].value, probes[2].value);
        } else {
            if (order <= 0 && residual_order <= 0 && improvement < 0) { status = found(&work, output, scratch[MID], &probes[0]); break; }
            bool replace_left = probes[0].sign == left_sign;
            status = calculator_from_rational_status(bigrational_copy(replace_left ? output->lower : output->upper, scratch[MID]));
            if (replace_left) left_sign = probes[0].sign; else right_sign = probes[0].sign;
            if (status == CALCULATOR_OK && brent) status = numeric_root_brent_accept(&interpolation, scratch[MID], probes[0].value);
        }
    }
    status = calculator_budget_status(status);
    if (status == CALCULATOR_OK) { output->evaluations = work.evaluations; *result = output; output = NULL; if (statistics) *statistics = stats; }
    else calculator_error_set(error, status, error ? error->offset : 0U);
    for (size_t i = 0U; i < 2U; ++i) application_expression_destroy(work.sides[i]);
    for (size_t i = 0U; i < SCRATCH_COUNT; ++i) bigrational_destroy(scratch[i]);
    for (size_t i = 0U; i < 3U; ++i) bigdecimal_destroy(probes[i].value);
    bigdecimal_destroy(absolute); bigdecimal_destroy(tolerance); bigdecimal_destroy(initial); free(copy); application_numeric_root_destroy(output);
    numeric_root_brent_destroy(&interpolation);
    if (owner) numforge_budget_end();
    return status;
}
CalculatorStatus application_numeric_root_solve(ApplicationNumericRoot **result, const char *input,
    const BigRational *lower, const BigRational *upper, const CalculatorContext *context, CalculatorError *error)
{
    return solve(result, input, lower, upper, context, error, false, NULL);
}
CalculatorStatus application_numeric_root_experiment(ApplicationNumericRoot **result, const char *input,
    const BigRational *lower, const BigRational *upper, const CalculatorContext *context, CalculatorError *error,
    bool brent, ApplicationNumericRootStats *statistics)
{
    return solve(result, input, lower, upper, context, error, brent, statistics);
}
