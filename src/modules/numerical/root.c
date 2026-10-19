#include "src/modules/numerical/root_internal.h"
#include <numforge/runtime.h>
#include <stdlib.h>

typedef struct Probe { BigDecimal *value; int sign; bool real, exact_zero; } Probe;
typedef struct Work {
    NumForgeRootEvaluate evaluate;
    NumForgeRootDomain domain;
    void *user;
    int64_t digits;
    size_t evaluations, max_evaluations;
    const BigDecimal *initial, *tolerance;
} Work;
static NumForgeNumericalStatus rational(BigRationalStatus status)
{
    switch (status) {
        case BIGRATIONAL_OK: return NUMFORGE_NUMERICAL_OK;
        case BIGRATIONAL_OUT_OF_MEMORY: return NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
        case BIGRATIONAL_VALUE_TOO_LARGE: return NUMFORGE_NUMERICAL_VALUE_TOO_LARGE;
        case BIGRATIONAL_SCALE_OVERFLOW: return NUMFORGE_NUMERICAL_SCALE_OVERFLOW;
        default: return NUMFORGE_NUMERICAL_INVALID_ARGUMENT;
    }
}
static NumForgeNumericalStatus decimal(BigDecimalStatus status)
{
    switch (status) {
        case BIGDECIMAL_OK: return NUMFORGE_NUMERICAL_OK;
        case BIGDECIMAL_OUT_OF_MEMORY: return NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
        case BIGDECIMAL_VALUE_TOO_LARGE: return NUMFORGE_NUMERICAL_VALUE_TOO_LARGE;
        case BIGDECIMAL_SCALE_OVERFLOW: return NUMFORGE_NUMERICAL_SCALE_OVERFLOW;
        default: return NUMFORGE_NUMERICAL_INVALID_ARGUMENT;
    }
}
void numforge_root_options_init(NumForgeRootOptions *options)
{
    if (options) {
        options->decimal_places = 34; options->max_iterations = 96;
        options->max_evaluations = 192; options->x_tolerance = NULL; options->residual_tolerance = NULL;
    }
}
void numforge_root_destroy(NumForgeRoot *result)
{
    if (!result) return;
    bigrational_destroy(result->x); bigrational_destroy(result->lower); bigrational_destroy(result->upper);
    bigdecimal_destroy(result->residual); free(result);
}
static NumForgeNumericalStatus probe(Work *work, const BigRational *x, int64_t digits, Probe *result)
{
    NumForgeRootSample sample = {false, false};
    result->real = false; result->exact_zero = false; result->sign = 0;
    ++work->evaluations;
    NumForgeNumericalStatus status = work->evaluate(result->value, &sample, x, digits, work->user);
    result->real = sample.real; result->exact_zero = sample.exact_zero;
    if (status == NUMFORGE_NUMERICAL_OK && sample.exact_zero && !sample.real) return NUMFORGE_NUMERICAL_INVALID_ARGUMENT;
    if (status == NUMFORGE_NUMERICAL_OK && sample.real) status = decimal(bigdecimal_sign(&result->sign, result->value));
    if (status == NUMFORGE_NUMERICAL_OK && sample.exact_zero && result->sign) return NUMFORGE_NUMERICAL_INVALID_ARGUMENT;
    return numforge_budget_check() ? status : NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
}
static NumForgeNumericalStatus midpoint(BigRational *result, const BigRational *a,
    const BigRational *b, const BigRational *two)
{
    NumForgeNumericalStatus status = rational(bigrational_add(result, a, b));
    return status == NUMFORGE_NUMERICAL_OK ? rational(bigrational_div(result, result, two)) : status;
}
static NumForgeNumericalStatus found(Work *work, NumForgeRoot *result,
    const BigRational *x, const Probe *sample)
{
    NumForgeNumericalStatus status = NUMFORGE_NUMERICAL_OK;
    Probe checked = {0};
    if (!sample->exact_zero) {
        bool verified = false;
        if (work->domain) status = work->domain(&verified, result->lower, result->upper, work->user);
        if (status != NUMFORGE_NUMERICAL_OK) return status;
        if (!verified) { result->kind = NUMFORGE_ROOT_DOMAIN_UNVERIFIED; return status; }
        if (work->evaluations >= work->max_evaluations) { result->kind = NUMFORGE_ROOT_ITERATION_LIMIT; return status; }
        checked.value = bigdecimal_create();
        if (!checked.value) status = NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
        if (status == NUMFORGE_NUMERICAL_OK) status = probe(work, x, work->digits * 2, &checked);
        if (status == NUMFORGE_NUMERICAL_OK && !checked.real) result->kind = NUMFORGE_ROOT_DOMAIN_GAP;
        if (status == NUMFORGE_NUMERICAL_OK && checked.real) {
            int order = 1;
            status = decimal(bigdecimal_abs(checked.value, checked.value));
            if (status == NUMFORGE_NUMERICAL_OK) status = decimal(bigdecimal_compare(&order, checked.value, work->tolerance));
            if (status == NUMFORGE_NUMERICAL_OK && (order > 0 || (sample->sign && checked.sign != sample->sign))) result->kind = NUMFORGE_ROOT_PRECISION_LIMIT;
            if (status == NUMFORGE_NUMERICAL_OK) status = decimal(bigdecimal_compare(&order, checked.value, work->initial));
            if (status == NUMFORGE_NUMERICAL_OK && order >= 0) result->kind = NUMFORGE_ROOT_PRECISION_LIMIT;
        }
        if (status != NUMFORGE_NUMERICAL_OK || result->kind != NUMFORGE_ROOT_ITERATION_LIMIT) {
            bigdecimal_destroy(checked.value); return status;
        }
    }
    result->x = bigrational_create(); result->residual = bigdecimal_create();
    status = result->x && result->residual ? NUMFORGE_NUMERICAL_OK : NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_copy(result->x, x));
    if (status == NUMFORGE_NUMERICAL_OK) status = decimal(bigdecimal_abs(result->residual, checked.value ? checked.value : sample->value));
    if (status == NUMFORGE_NUMERICAL_OK) { result->kind = NUMFORGE_ROOT_FOUND; result->exact_zero = sample->exact_zero; }
    bigdecimal_destroy(checked.value); return status;
}

NumForgeNumericalStatus numforge_root_solve_strategy(NumForgeRoot **result,
    NumForgeRootEvaluate evaluate, NumForgeRootDomain domain, void *user,
    const BigRational *lower, const BigRational *upper, const NumForgeRootOptions *options,
    const NumForgeRootStrategy *strategy)
{
    if (!result || !evaluate || !lower || !upper) return NUMFORGE_NUMERICAL_NULL_ARGUMENT;
    NumForgeRootOptions defaults; numforge_root_options_init(&defaults);
    if (!options) options = &defaults;
    if (options->decimal_places < 1 || options->decimal_places > INT64_MAX/2 ||
        !options->max_iterations || options->max_evaluations < 4) return NUMFORGE_NUMERICAL_INVALID_ARGUMENT;
    if (strategy && (!strategy->reset || !strategy->propose || !strategy->accept)) return NUMFORGE_NUMERICAL_INVALID_ARGUMENT;
    NumForgeNumericalStatus status = NUMFORGE_NUMERICAL_OK;
    Work work = {evaluate, domain, user, options->decimal_places, 0, options->max_evaluations, NULL, NULL};
    NumForgeRoot *output = numforge_calloc(1, sizeof(*output));
    enum { MID, LEFT_QUARTER, RIGHT_QUARTER, TWO, WIDTH, TOLERANCE, SCRATCH_COUNT };
    BigRational *scratch[SCRATCH_COUNT] = {0};
    Probe probes[3] = {{0}};
    BigDecimal *absolute = NULL, *tolerance = NULL, *initial = NULL;
    int order = 0, left_sign = 0, right_sign = 0;
    if (!output) status = NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    if (status == NUMFORGE_NUMERICAL_OK) {
        output->kind = NUMFORGE_ROOT_ITERATION_LIMIT;
        output->lower = bigrational_create(); output->upper = bigrational_create();
        absolute = bigdecimal_create(); tolerance = bigdecimal_create(); initial = bigdecimal_create();
        work.initial = initial; work.tolerance = tolerance;
        if (!output->lower || !output->upper || !absolute || !tolerance || !initial) status = NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    }
    for (size_t i = 0; i < SCRATCH_COUNT && status == NUMFORGE_NUMERICAL_OK; ++i) {
        scratch[i] = bigrational_create(); if (!scratch[i]) status = NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    }
    for (size_t i = 0; i < 3 && status == NUMFORGE_NUMERICAL_OK; ++i) {
        probes[i].value = bigdecimal_create(); if (!probes[i].value) status = NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    }
    if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_compare(&order, lower, upper));
    if (status == NUMFORGE_NUMERICAL_OK && order >= 0) status = NUMFORGE_NUMERICAL_INVALID_ARGUMENT;
    if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_copy(output->lower, lower));
    if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_copy(output->upper, upper));
    if (status == NUMFORGE_NUMERICAL_OK) status = options->residual_tolerance ?
        decimal(bigdecimal_copy(tolerance, options->residual_tolerance)) : decimal(bigdecimal_set_string(tolerance, "1E-12"));
    if (status == NUMFORGE_NUMERICAL_OK) status = decimal(bigdecimal_sign(&order, tolerance));
    if (status == NUMFORGE_NUMERICAL_OK && order <= 0) status = NUMFORGE_NUMERICAL_INVALID_ARGUMENT;
    if (status == NUMFORGE_NUMERICAL_OK) status = options->x_tolerance ?
        rational(bigrational_copy(scratch[TOLERANCE], options->x_tolerance)) : rational(bigrational_from_bigdecimal(scratch[TOLERANCE], tolerance));
    /* Width defaults independently of a caller's residual tolerance. */
    if (status == NUMFORGE_NUMERICAL_OK && !options->x_tolerance && options->residual_tolerance) {
        status = decimal(bigdecimal_set_string(absolute, "1E-12"));
        if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_from_bigdecimal(scratch[TOLERANCE], absolute));
    }
    if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_compare(&order, scratch[TOLERANCE], scratch[WIDTH]));
    if (status == NUMFORGE_NUMERICAL_OK && order <= 0) status = NUMFORGE_NUMERICAL_INVALID_ARGUMENT;
    if (status == NUMFORGE_NUMERICAL_OK) status = decimal(bigdecimal_set_string(absolute, "2"));
    if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_from_bigdecimal(scratch[TWO], absolute));
    for (size_t i = 0; i < 2 && status == NUMFORGE_NUMERICAL_OK; ++i) {
        status = probe(&work, i ? upper : lower, work.digits, &probes[i]);
        if (status == NUMFORGE_NUMERICAL_OK && !probes[i].real) { output->kind = NUMFORGE_ROOT_DOMAIN_GAP; break; }
        if (status == NUMFORGE_NUMERICAL_OK && probes[i].exact_zero) { status = found(&work, output, i ? upper : lower, &probes[i]); break; }
        if (!i) left_sign = probes[i].sign; else right_sign = probes[i].sign;
    }
    if (status == NUMFORGE_NUMERICAL_OK && output->kind == NUMFORGE_ROOT_ITERATION_LIMIT &&
        (!left_sign || !right_sign || left_sign == right_sign)) output->kind = NUMFORGE_ROOT_NOT_BRACKETED;
    if (status == NUMFORGE_NUMERICAL_OK && output->kind == NUMFORGE_ROOT_ITERATION_LIMIT) {
        status = decimal(bigdecimal_abs(initial, probes[0].value));
        if (status == NUMFORGE_NUMERICAL_OK) status = decimal(bigdecimal_abs(absolute, probes[1].value));
        if (status == NUMFORGE_NUMERICAL_OK) status = decimal(bigdecimal_compare(&order, absolute, initial));
        if (status == NUMFORGE_NUMERICAL_OK && order < 0) status = decimal(bigdecimal_copy(initial, absolute));
        if (status == NUMFORGE_NUMERICAL_OK && strategy) status = strategy->reset(lower, upper, probes[0].value, probes[1].value, strategy->user);
    }
    for (size_t iteration = 0; iteration < options->max_iterations && status == NUMFORGE_NUMERICAL_OK &&
        output->kind == NUMFORGE_ROOT_ITERATION_LIMIT && work.evaluations < options->max_evaluations-3; ++iteration) {
        ++output->iterations;
        bool proposed = false;
        if (strategy) status = strategy->propose(scratch[MID], &proposed, strategy->user);
        if (status == NUMFORGE_NUMERICAL_OK && !proposed) status = midpoint(scratch[MID], output->lower, output->upper, scratch[TWO]);
        if (status == NUMFORGE_NUMERICAL_OK) status = probe(&work, scratch[MID], work.digits, &probes[0]);
        if (status != NUMFORGE_NUMERICAL_OK) break;
        if (!probes[0].real) { output->kind = NUMFORGE_ROOT_DOMAIN_GAP; break; }
        if (probes[0].exact_zero) { status = found(&work, output, scratch[MID], &probes[0]); break; }
        status = rational(bigrational_sub(scratch[WIDTH], output->upper, output->lower));
        if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_compare(&order, scratch[WIDTH], scratch[TOLERANCE]));
        int residual_order = 1, improvement = 1;
        if (status == NUMFORGE_NUMERICAL_OK) status = decimal(bigdecimal_abs(absolute, probes[0].value));
        if (status == NUMFORGE_NUMERICAL_OK) status = decimal(bigdecimal_compare(&residual_order, absolute, tolerance));
        if (status == NUMFORGE_NUMERICAL_OK) status = decimal(bigdecimal_compare(&improvement, absolute, initial));
        if (status != NUMFORGE_NUMERICAL_OK) break;
        if (!probes[0].sign) {
            status = midpoint(scratch[LEFT_QUARTER], output->lower, scratch[MID], scratch[TWO]);
            if (status == NUMFORGE_NUMERICAL_OK) status = midpoint(scratch[RIGHT_QUARTER], scratch[MID], output->upper, scratch[TWO]);
            if (status == NUMFORGE_NUMERICAL_OK) status = probe(&work, scratch[LEFT_QUARTER], work.digits, &probes[1]);
            if (status == NUMFORGE_NUMERICAL_OK) status = probe(&work, scratch[RIGHT_QUARTER], work.digits, &probes[2]);
            if (status != NUMFORGE_NUMERICAL_OK) break;
            if (!probes[1].real || !probes[2].real) { output->kind = NUMFORGE_ROOT_DOMAIN_GAP; break; }
            if (!probes[1].sign || !probes[2].sign || probes[1].sign == probes[2].sign) { output->kind = NUMFORGE_ROOT_PRECISION_LIMIT; break; }
            if (order <= 0 && residual_order <= 0 && improvement < 0) { status = found(&work, output, scratch[MID], &probes[0]); break; }
            left_sign = probes[1].sign; right_sign = probes[2].sign;
            status = rational(bigrational_copy(output->lower, scratch[LEFT_QUARTER]));
            if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_copy(output->upper, scratch[RIGHT_QUARTER]));
            if (status == NUMFORGE_NUMERICAL_OK && strategy) status = strategy->reset(output->lower, output->upper, probes[1].value, probes[2].value, strategy->user);
        } else {
            if (order <= 0 && residual_order <= 0 && improvement < 0) { status = found(&work, output, scratch[MID], &probes[0]); break; }
            bool replace_left = probes[0].sign == left_sign;
            status = rational(bigrational_copy(replace_left ? output->lower : output->upper, scratch[MID]));
            if (replace_left) left_sign = probes[0].sign; else right_sign = probes[0].sign;
            if (status == NUMFORGE_NUMERICAL_OK && strategy) status = strategy->accept(scratch[MID], probes[0].value, strategy->user);
        }
    }
    if (!numforge_budget_check()) status = NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    if (status == NUMFORGE_NUMERICAL_OK) { output->evaluations = work.evaluations; *result = output; output = NULL; }
    for (size_t i = 0; i < SCRATCH_COUNT; ++i) bigrational_destroy(scratch[i]);
    for (size_t i = 0; i < 3; ++i) bigdecimal_destroy(probes[i].value);
    bigdecimal_destroy(absolute); bigdecimal_destroy(tolerance); bigdecimal_destroy(initial); numforge_root_destroy(output);
    return status;
}
NumForgeNumericalStatus numforge_root_solve(NumForgeRoot **result,
    NumForgeRootEvaluate evaluate, NumForgeRootDomain domain, void *user,
    const BigRational *lower, const BigRational *upper, const NumForgeRootOptions *options)
{ return numforge_root_solve_strategy(result, evaluate, domain, user, lower, upper, options, NULL); }
NumForgeRootKind numforge_root_kind(const NumForgeRoot *r) { return r ? r->kind : NUMFORGE_ROOT_ITERATION_LIMIT; }
const BigRational *numforge_root_x(const NumForgeRoot *r) { return r ? r->x : NULL; }
const BigRational *numforge_root_lower(const NumForgeRoot *r) { return r ? r->lower : NULL; }
const BigRational *numforge_root_upper(const NumForgeRoot *r) { return r ? r->upper : NULL; }
const BigDecimal *numforge_root_residual(const NumForgeRoot *r) { return r ? r->residual : NULL; }
size_t numforge_root_evaluations(const NumForgeRoot *r) { return r ? r->evaluations : 0; }
size_t numforge_root_iterations(const NumForgeRoot *r) { return r ? r->iterations : 0; }
bool numforge_root_exact_zero(const NumForgeRoot *r) { return r && r->exact_zero; }
