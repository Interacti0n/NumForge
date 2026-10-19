#include "src/application/equations/numeric_root.h"
#include "src/application/numerical_status.h"
#include "src/modules/numerical/root_strategy_internal.h"
#include "src/application/equations/numeric_root_brent_internal.h"
#include "src/application/expressions/expression.h"
#include "src/engine/evaluator/exact_functions.h"
#include "src/runtime/numforge_alloc.h"
#include <stdlib.h>
#include <string.h>

/* Optional rational ordinates are scan proposal data, never acceptance proof.
 * Single-bracket probes leave proposal_value NULL and retain their old cost. */
typedef struct Probe { BigDecimal *value; bool real, exact_zero; BigRational *proposal_value; } Probe;
typedef struct Work { ApplicationExpression *sides[2]; const char *sources[2]; char *source; CalculatorStatus callback_status; CalculatorError *error; CalculatorContext context; size_t rhs; } Work;

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
    numforge_root_destroy(result);
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
    result->real = false; result->exact_zero = false;
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
        result->real = samples[0]->error.status == CALCULATOR_OK && samples[1]->error.status == CALCULATOR_OK &&
            !samples[0]->value.quantity && !samples[1]->value.quantity &&
            !calculator_value_is_complex(&samples[0]->value) && !calculator_value_is_complex(&samples[1]->value);
        if (result->real && exact(&samples[0]->value) && exact(&samples[1]->value)) {
            values[0] = bigrational_create(); values[1] = bigrational_create();
            if (!values[0] || !values[1]) status = CALCULATOR_OUT_OF_MEMORY;
            for (size_t i = 0U; i < 2U && status == CALCULATOR_OK; ++i) status = rational(values[i], &samples[i]->value);
            if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_sub(values[0], values[0], values[1]));
            if (status == CALCULATOR_OK && result->proposal_value) status = calculator_from_rational_status(bigrational_copy(result->proposal_value, values[0]));
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
        } else if (result->real) {
            status = calculator_from_decimal_status(bigdecimal_sub(result->value, samples[0]->value.number, samples[1]->value.number));
            if (status == CALCULATOR_OK && result->proposal_value) status = calculator_from_rational_status(bigrational_from_bigdecimal(result->proposal_value, result->value));
        }
    }
    for (size_t i = 0U; i < 2U; ++i) { application_expression_samples_destroy(samples[i], 1U); bigrational_destroy(values[i]); }
    calculator_value_destroy(&argument); return status;
}
static NumForgeNumericalStatus callback_status(Work *work, CalculatorStatus status)
{
    if (status == CALCULATOR_OK) return NUMFORGE_NUMERICAL_OK;
    work->callback_status = status; return NUMFORGE_NUMERICAL_CALLBACK_FAILED;
}
static NumForgeNumericalStatus scan_evaluate_callback(BigDecimal *value, BigRational *proposal_value, NumForgeRootSample *sample,
    const BigRational *x, int64_t digits, void *user)
{
    Work *work = user; Probe sampled = {0}; sampled.value = value; sampled.proposal_value = proposal_value;
    CalculatorStatus status = CALCULATOR_OK;
    if (digits == work->context.division_scale) status = probe(work, x, &sampled, work->error);
    else {
        Work verification = {0}; verification.context = work->context;
        verification.context.division_scale = digits; verification.rhs = work->rhs;
        for (size_t i = 0; i < 2 && status == CALCULATOR_OK; ++i) {
            CalculatorError ignored;
            status = application_expression_prepare(&verification.sides[i], work->sources[i], "x", NULL, &verification.context, &ignored);
        }
        if (status == CALCULATOR_OK) status = probe(&verification, x, &sampled, NULL);
        for (size_t i = 0; i < 2; ++i) application_expression_destroy(verification.sides[i]);
    }
    sample->real = sampled.real; sample->exact_zero = sampled.exact_zero;
    return callback_status(work, status);
}
static NumForgeNumericalStatus evaluate_callback(BigDecimal *value, NumForgeRootSample *sample,
    const BigRational *x, int64_t digits, void *user)
{ return scan_evaluate_callback(value, NULL, sample, x, digits, user); }
static NumForgeNumericalStatus domain_callback(bool *verified,
    const BigRational *lower, const BigRational *upper, void *user)
{
    Work *work = user; *verified = false;
    for (size_t i = 0; i < 2; ++i) {
        bool side = false;
        CalculatorStatus status = application_expression_domain(&side, work->sides[i], lower, upper);
        if (status != CALCULATOR_OK) return callback_status(work, status);
        if (!side) return NUMFORGE_NUMERICAL_OK;
    }
    *verified = true; return NUMFORGE_NUMERICAL_OK;
}
typedef struct StrategyWork {
    Work *work; NumericRootBrent interpolation;
    ApplicationNumericRootStats stats;
    bool hybrid;
} StrategyWork;
static NumForgeNumericalStatus strategy_reset(const BigRational *lower, const BigRational *upper,
    const BigDecimal *left, const BigDecimal *right, void *user)
{
    StrategyWork *strategy = user; CalculatorStatus status = CALCULATOR_OK;
    if (!strategy->interpolation.a) status = numeric_root_brent_create(&strategy->interpolation,
        strategy->work->context.division_scale, strategy->hybrid);
    if (status == CALCULATOR_OK) status = numeric_root_brent_reset(&strategy->interpolation, lower, upper, left, right);
    return callback_status(strategy->work, status);
}
static NumForgeNumericalStatus strategy_propose(BigRational *x, bool *used, void *user)
{
    StrategyWork *strategy = user; *used = !strategy->interpolation.disabled;
    if (!*used) { ++strategy->stats.bisections; ++strategy->stats.fallback_steps; return NUMFORGE_NUMERICAL_OK; }
    return callback_status(strategy->work, numeric_root_brent_propose(&strategy->interpolation, x, &strategy->stats));
}
static NumForgeNumericalStatus strategy_accept(const BigRational *x, const BigDecimal *value, void *user)
{
    StrategyWork *strategy = user;
    return strategy->interpolation.disabled ? NUMFORGE_NUMERICAL_OK :
        callback_status(strategy->work, numeric_root_brent_accept(&strategy->interpolation, x, value, &strategy->stats));
}
/* Application ownership only: the numerical kernels never parse or own sources. */
static CalculatorStatus prepare_work(Work *work, const char *input, size_t length, size_t rhs,
    const CalculatorContext *context, CalculatorError *error)
{
    work->context = *context; work->context.division_scale += 16;
    work->context.significant_division = true; work->error = error; work->rhs = rhs;
    work->source = numforge_malloc(length+1);
    if (!work->source) return CALCULATOR_OUT_OF_MEMORY;
    memcpy(work->source, input, length+1); work->source[rhs-1] = '\0';
    work->sources[0] = work->source; work->sources[1] = work->source+rhs;
    CalculatorStatus status = CALCULATOR_OK;
    for (size_t i = 0; i < 2 && status == CALCULATOR_OK; ++i) {
        status = application_expression_prepare(&work->sides[i], work->sources[i], "x", NULL, &work->context, error);
        if (status != CALCULATOR_OK && i) calculator_error_set(error, status, rhs+(error ? error->offset : 0));
    }
    return status;
}
static void destroy_work(Work *work)
{
    for (size_t i = 0; i < 2; ++i) application_expression_destroy(work->sides[i]);
    free(work->source);
}
static CalculatorStatus solve(ApplicationNumericRoot **result, const char *input,
    const BigRational *lower, const BigRational *upper, const CalculatorContext *context, CalculatorError *error,
    ApplicationNumericRootMethod method, ApplicationNumericRootStats *statistics)
{
    calculator_error_clear(error);
    if ((unsigned)method > APPLICATION_ROOT_DECIMAL_HYBRID) { calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, 0U); return CALCULATOR_INVALID_ARGUMENT; }
    bool brent = method != APPLICATION_ROOT_BISECTION;
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
    Work work = {0};
    status = prepare_work(&work, input, length, (size_t)(equal-input)+1, context, error);
    StrategyWork strategy_work = {0}; strategy_work.work = &work;
    strategy_work.hybrid = method == APPLICATION_ROOT_DECIMAL_HYBRID;
    NumForgeRootStrategy strategy = {strategy_reset, strategy_propose, strategy_accept, &strategy_work};
    NumForgeRootOptions options; numforge_root_options_init(&options);
    options.decimal_places = work.context.division_scale;
    ApplicationNumericRoot *output = NULL;
    if (status == CALCULATOR_OK) {
        NumForgeNumericalStatus numerical = brent ?
            numforge_root_solve_strategy(&output, evaluate_callback, domain_callback, &work, lower, upper, &options, &strategy) :
            numforge_root_solve(&output, evaluate_callback, domain_callback, &work, lower, upper, &options);
        status = application_from_numerical_status(numerical, work.callback_status);
    }
    status = calculator_budget_status(status);
    if (status == CALCULATOR_OK) {
        if (!brent) strategy_work.stats.bisections = numforge_root_iterations(output);
        *result = output; output = NULL;
        if (statistics) *statistics = strategy_work.stats;
    } else calculator_error_set(error, status, error ? error->offset : 0U);
    destroy_work(&work);
    numeric_root_brent_destroy(&strategy_work.interpolation);
    application_numeric_root_destroy(output);
    if (owner) numforge_budget_end();
    return status;
}

CalculatorStatus application_numeric_root_solve(ApplicationNumericRoot **result, const char *input,
    const BigRational *lower, const BigRational *upper, const CalculatorContext *context, CalculatorError *error)
{
    return solve(result, input, lower, upper, context, error, APPLICATION_ROOT_BISECTION, NULL);
}
CalculatorStatus application_numeric_root_experiment(ApplicationNumericRoot **result, const char *input,
    const BigRational *lower, const BigRational *upper, const CalculatorContext *context, CalculatorError *error,
    bool brent, ApplicationNumericRootStats *statistics)
{
    return solve(result, input, lower, upper, context, error, brent ? APPLICATION_ROOT_RATIONAL_BRENT : APPLICATION_ROOT_BISECTION, statistics);
}
CalculatorStatus application_numeric_root_method(ApplicationNumericRoot **result, const char *input,
    const BigRational *lower, const BigRational *upper, const CalculatorContext *context, CalculatorError *error,
    ApplicationNumericRootMethod method, ApplicationNumericRootStats *statistics)
{
    return solve(result, input, lower, upper, context, error, method, statistics);
}

void application_numeric_roots_destroy(ApplicationRootScan *result)
{ numforge_root_scan_destroy(result); }
const char *application_numeric_roots_stop(ApplicationRootScanStop stop)
{
    switch (stop) {
        case APPLICATION_SCAN_FINISHED: return "scanned";
        case APPLICATION_SCAN_ROOT_LIMIT: return "root_limit";
        case APPLICATION_SCAN_EVALUATION_LIMIT: return "evaluation_limit";
        default: return "evaluation_limit";
    }
}
CalculatorStatus application_numeric_roots_scan(ApplicationRootScan **result, const char *input,
    const BigRational *lower, const BigRational *upper, const CalculatorContext *context, CalculatorError *error)
{
    calculator_error_clear(error);
    if (!result || !input || !lower || !upper || !context) {
        calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0U); return CALCULATOR_NULL_ARGUMENT;
    }
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
    Work work = {0};
    status = prepare_work(&work, input, length, (size_t)(equal-input)+1, context, error);
    NumForgeRootScanOptions options; numforge_root_scan_options_init(&options);
    options.root_options.decimal_places = work.context.division_scale;
    options.segments = APPLICATION_ROOT_SCAN_SEGMENTS; options.max_roots = APPLICATION_ROOT_SCAN_MAX_ROOTS;
    options.max_evaluations = APPLICATION_ROOT_SCAN_MAX_EVALUATIONS;
    ApplicationRootScan *output = NULL;
    if (status == CALCULATOR_OK) {
        NumForgeNumericalStatus numerical = numforge_roots_scan(&output, scan_evaluate_callback, domain_callback,
            &work, lower, upper, &options);
        status = application_from_numerical_status(numerical, work.callback_status);
    }
    status = calculator_budget_status(status);
    if (status == CALCULATOR_OK) { *result = output; output = NULL; }
    else calculator_error_set(error, status, error ? error->offset : 0U);
    destroy_work(&work); application_numeric_roots_destroy(output);
    if (owner) numforge_budget_end();
    return status;
}
