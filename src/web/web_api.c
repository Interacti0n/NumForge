#include "web_api.h"
#include "formatter.h"

#include <numforge/runtime.h>
#include <stdlib.h>
#include <string.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Local web adapter operations. This module intentionally contains no HTTP or
    platform code, so its exact C-core behaviour can be covered by unit tests.
------------------------------------------------------------------------------------------------------------------------------
*/

char *numforge_web_fraction_approximation(const char *display)
{
    char fraction[17];
    char *separator;
    char *approximation = NULL;
    BigInt *numerator = NULL;
    BigInt *denominator = NULL;
    BigRational *rational = NULL;
    BigDecimal *decimal = NULL;
    size_t length;

    if (display == NULL || (length = strlen(display)) > 16U)
    {
        return NULL;
    }
    memcpy(fraction, display, length + 1U);
    separator = strchr(fraction, '/');
    if (separator == NULL || separator == fraction || separator[1] == '\0')
    {
        return NULL;
    }
    *separator = '\0';
    numerator = bigint_create();
    denominator = bigint_create();
    rational = bigrational_create();
    decimal = bigdecimal_create();
    if (numerator != NULL && denominator != NULL && rational != NULL && decimal != NULL &&
        bigint_set_string(numerator, fraction) == BIGINT_OK &&
        bigint_set_string(denominator, separator + 1U) == BIGINT_OK &&
        bigrational_set_fraction(rational, numerator, denominator) == BIGRATIONAL_OK &&
        bigrational_to_bigdecimal(decimal, rational, 34, BIGDECIMAL_ROUND_HALF_EVEN) == BIGRATIONAL_OK)
    {
        (void)bigdecimal_format_mode(decimal, 10, BIGDECIMAL_ROUND_HALF_EVEN,
            BIGDECIMAL_FORMAT_AUTO, 80U, &approximation);
        if (approximation != NULL && strcmp(approximation, "0") == 0)
        {
            free(approximation);
            approximation = NULL;
            (void)bigdecimal_format_mode(decimal, 10, BIGDECIMAL_ROUND_HALF_EVEN,
                BIGDECIMAL_FORMAT_SCIENTIFIC, 80U, &approximation);
        }
    }
    bigdecimal_destroy(decimal);
    bigrational_destroy(rational);
    bigint_destroy(denominator);
    bigint_destroy(numerator);
    return approximation;
}

CalculatorStatus numforge_web_evaluate(
    const char *input,
    char **result,
    CalculatorError *error
)
{
    return numforge_web_evaluate_with_output_scale(input, CALCULATOR_DEFAULT_OUTPUT_SCALE, result, error);
}

CalculatorStatus numforge_web_evaluate_session(
    CalculatorSession *session,
    uint64_t revision,
    bool commit,
    const char *input,
    int64_t output_scale,
    CalculatorAngleUnit angle_unit,
    char **result,
    CalculatorError *error,
    bool *reused
)
{
    return numforge_web_evaluate_session_mode(session, revision, commit, input,
        output_scale, angle_unit, CALCULATOR_NOTATION_AUTO, result, error, reused);
}

CalculatorStatus numforge_web_evaluate_session_mode(
    CalculatorSession *session, uint64_t revision, bool commit, const char *input,
    int64_t output_scale, CalculatorAngleUnit angle_unit, CalculatorNotation notation,
    char **result, CalculatorError *error, bool *reused
)
{
    return numforge_web_evaluate_session_form(session, revision, commit, input, output_scale,
        angle_unit, notation, BIGCOMPLEX_FORM_CARTESIAN, result, error, reused);
}
CalculatorStatus numforge_web_evaluate_session_form(CalculatorSession *session,
    uint64_t revision, bool commit, const char *input, int64_t output_scale,
    CalculatorAngleUnit angle_unit, CalculatorNotation notation, BigComplexForm form,
    char **result, CalculatorError *error, bool *reused)
{
    CalculatorContext context;
    CalculatorStatus status;

    if (result != NULL)
    {
        *result = NULL;
    }
    if (reused != NULL)
    {
        *reused = false;
    }
    calculator_context_init(&context);
    status = calculator_context_set_output_scale(&context, output_scale);
    if (status == CALCULATOR_OK)
    {
        status = calculator_context_set_angle_unit(&context, angle_unit);
    }
    if (status != CALCULATOR_OK)
    {
        calculator_error_set(error, status, 0U);
        return status;
    }

    context.notation = notation;
    context.complex_form = form;

    return calculator_session_compute(session, revision, commit, input, &context, result, error, reused);
}

CalculatorStatus numforge_web_evaluate_with_output_scale(
    const char *input,
    int64_t output_scale,
    char **result,
    CalculatorError *error
)
{
    return numforge_web_evaluate_with_options(
        input, output_scale, CALCULATOR_ANGLE_RADIANS, result, error);
}

CalculatorStatus numforge_web_evaluate_with_options(
    const char *input,
    int64_t output_scale,
    CalculatorAngleUnit angle_unit,
    char **result,
    CalculatorError *error
)
{
    return numforge_web_evaluate_cached(NULL, 0U, input, output_scale, angle_unit, result, error, NULL);
}

void numforge_web_cache_clear(NumForgeWebCache *cache)
{
    application_evaluation_cache_clear(cache);
}

CalculatorStatus numforge_web_evaluate_cached(
    NumForgeWebCache *cache,
    uint64_t revision,
    const char *input,
    int64_t output_scale,
    CalculatorAngleUnit angle_unit,
    char **result,
    CalculatorError *error,
    bool *reused
)
{
    return numforge_web_evaluate_cached_mode(cache, revision, input, output_scale,
        angle_unit, CALCULATOR_NOTATION_AUTO, result, error, reused);
}

CalculatorStatus numforge_web_evaluate_cached_mode(
    NumForgeWebCache *cache, uint64_t revision, const char *input,
    int64_t output_scale, CalculatorAngleUnit angle_unit, CalculatorNotation notation,
    char **result, CalculatorError *error, bool *reused
)
{
    return numforge_web_evaluate_cached_form(cache, revision, input, output_scale, angle_unit,
        notation, BIGCOMPLEX_FORM_CARTESIAN, result, error, reused);
}
CalculatorStatus numforge_web_evaluate_cached_form(NumForgeWebCache *cache,
    uint64_t revision, const char *input, int64_t output_scale,
    CalculatorAngleUnit angle_unit, CalculatorNotation notation, BigComplexForm form,
    char **result, CalculatorError *error, bool *reused)
{
    CalculatorContext context;
    CalculatorStatus status;
    CalculatorValue value = {0};
    bool owner;
    bool replace = false;
    size_t length;

    if (reused != NULL)
    {
        *reused = false;
    }
    if (cache != NULL && revision > cache->revision)
    {
        cache->revision = revision;
        replace = true;
    }

    if (result != NULL)
    {
        *result = NULL;
    }

    if (input == NULL || result == NULL)
    {
        calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0);

        return CALCULATOR_NULL_ARGUMENT;
    }

    length = 0U;

    while (length <= NUMFORGE_WEB_MAX_EXPRESSION_LENGTH && input[length] != '\0')
    {
        length++;
    }

    if (length > NUMFORGE_WEB_MAX_EXPRESSION_LENGTH)
    {
        calculator_error_set(error, CALCULATOR_VALUE_TOO_LARGE, length);

        return CALCULATOR_VALUE_TOO_LARGE;
    }

    calculator_context_init(&context);
    status = calculator_context_set_output_scale(&context, output_scale);

    if (status != CALCULATOR_OK)
    {
        calculator_error_set(error, status, 0);

        return status;
    }

    status = calculator_context_set_angle_unit(&context, angle_unit);

    if (status != CALCULATOR_OK)
    {
        calculator_error_set(error, status, 0);

        return status;
    }

    context.notation = notation;
    context.complex_form = form;

    owner = numforge_budget_begin(
        (uint64_t)context.time_limit_ms, CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    if (cache != NULL && !cache->value.uses_random && strcmp(cache->expression, input) == 0 &&
        calculator_value_matches(&cache->value, &context))
    {
        status = calculator_format_value(&cache->value, &context, result);
        calculator_error_set(error, status, 0U);
        if (reused != NULL)
        {
            *reused = status == CALCULATOR_OK;
        }
    }
    else
    {
        status = calculator_compute_value(input, &context, &value, error);
        if (status == CALCULATOR_OK)
        {
            status = calculator_format_value(&value, &context, result);
            calculator_error_set(error, status, 0U);
        }
        if (status == CALCULATOR_OK && replace && !value.uses_random)
        {
            calculator_value_destroy(&cache->value);
            cache->value = value;
            value.number = NULL;
            value.integer = NULL;
            value.rational = NULL;
            value.complex_decimal = NULL;
            value.complex_rational = NULL;
            memcpy(cache->expression, input, length + 1U);
        }
    }
    calculator_value_destroy(&value);
    if (owner)
    {
        numforge_budget_end();
    }
    return status;
}
