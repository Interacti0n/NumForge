#include "value_internal.h"
#include "formatter.h"

#include <numforge/runtime.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* Typed value ownership, exact projection, and output selection. */

void calculator_value_destroy(CalculatorValue *value)
{
    if (value != NULL)
    {
        bigdecimal_destroy(value->number);
        bigint_destroy(value->integer);
        bigrational_destroy(value->rational);
        memset(value, 0, sizeof(*value));
    }
}

CalculatorStatus calculator_value_copy(CalculatorValue *result, const CalculatorValue *value)
{
    CalculatorValue temporary = {0};
    if (result == NULL || value == NULL)
    {
        return CALCULATOR_NULL_ARGUMENT;
    }
    temporary.number = bigdecimal_create();
    if (temporary.number == NULL || bigdecimal_copy(temporary.number, value->number) != BIGDECIMAL_OK)
    {
        goto failed;
    }
    if (value->integer != NULL)
    {
        temporary.integer = bigint_create();
        if (temporary.integer == NULL || bigint_copy(temporary.integer, value->integer) != BIGINT_OK)
        {
            goto failed;
        }
    }
    if (value->rational != NULL)
    {
        temporary.rational = bigrational_create();
        if (temporary.rational == NULL || bigrational_copy(temporary.rational, value->rational) != BIGRATIONAL_OK)
        {
            goto failed;
        }
    }
    temporary.context = value->context;
    temporary.kind = value->kind;
    temporary.independent = value->independent;
    temporary.uses_answer = value->uses_answer;
    temporary.uses_random = value->uses_random;
    calculator_value_destroy(result);
    *result = temporary;
    return CALCULATOR_OK;
failed:
    calculator_value_destroy(&temporary);
    return CALCULATOR_OUT_OF_MEMORY;
}

CalculatorStatus calculator_materialize_exact(
    BigDecimal *result, const CalculatorValue *value, const CalculatorContext *context)
{
    if (value->kind == CALCULATOR_VALUE_INTEGER)
    {
        BigDecimalStatus decimal_status = bigdecimal_from_bigint(result, value->integer);
        return decimal_status == BIGDECIMAL_OK ? CALCULATOR_OK :
            decimal_status == BIGDECIMAL_OUT_OF_MEMORY ? CALCULATOR_OUT_OF_MEMORY : CALCULATOR_VALUE_TOO_LARGE;
    }
    {
        BigRationalStatus rational_status = bigrational_to_bigdecimal(
            result, value->rational, context->division_scale, context->rounding);
        return rational_status == BIGRATIONAL_OK ? CALCULATOR_OK :
            rational_status == BIGRATIONAL_OUT_OF_MEMORY ? CALCULATOR_OUT_OF_MEMORY :
            rational_status == BIGRATIONAL_SCALE_OVERFLOW ? CALCULATOR_SCALE_OVERFLOW :
            rational_status == BIGRATIONAL_INVALID_ARGUMENT ? CALCULATOR_INVALID_ARGUMENT :
            CALCULATOR_VALUE_TOO_LARGE;
    }
}

static CalculatorStatus calculator_format_exact_text(
    const CalculatorValue *value, const CalculatorContext *context, char **result, bool *too_long)
{
    bool owner = numforge_budget_begin(
        (uint64_t)context->time_limit_ms, CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    CalculatorStatus status;

    if (too_long != NULL)
    {
        *too_long = false;
    }
    if (value->kind == CALCULATOR_VALUE_INTEGER)
    {
        *result = bigint_to_string(value->integer);
        status = *result == NULL ? CALCULATOR_OUT_OF_MEMORY : CALCULATOR_OK;
    }
    else
    {
        BigRationalStatus rational_status = bigrational_to_string(value->rational, result);
        status = rational_status == BIGRATIONAL_OK ? CALCULATOR_OK :
            rational_status == BIGRATIONAL_OUT_OF_MEMORY ? CALCULATOR_OUT_OF_MEMORY :
            rational_status == BIGRATIONAL_VALUE_TOO_LARGE ? CALCULATOR_VALUE_TOO_LARGE :
            CALCULATOR_INVALID_ARGUMENT;
    }
    status = calculator_budget_status(status);
    if (status == CALCULATOR_OK && strlen(*result) > CALCULATOR_MAX_OUTPUT_BYTES)
    {
        status = CALCULATOR_VALUE_TOO_LARGE;
        if (too_long != NULL)
        {
            *too_long = true;
        }
    }
    if (status != CALCULATOR_OK)
    {
        free(*result);
        *result = NULL;
    }
    if (owner)
    {
        numforge_budget_end();
    }
    return status;
}

static CalculatorStatus calculator_fraction_candidate(
    const BigRational *value, const CalculatorContext *context, bool *candidate)
{
    bool owner = numforge_budget_begin(
        (uint64_t)context->time_limit_ms, CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    BigInt *numerator = bigint_create();
    BigInt *denominator = bigint_create();
    BigInt *limit = bigint_create();
    BigInt *negative_limit = bigint_create();
    CalculatorStatus status = numerator == NULL || denominator == NULL || limit == NULL ||
        negative_limit == NULL ? CALCULATOR_OUT_OF_MEMORY : CALCULATOR_OK;

    *candidate = false;
    if (status == CALCULATOR_OK)
    {
        status = bigrational_get_numerator(numerator, value) == BIGRATIONAL_OK &&
            bigrational_get_denominator(denominator, value) == BIGRATIONAL_OK ?
            CALCULATOR_OK : CALCULATOR_OUT_OF_MEMORY;
    }
    if (status == CALCULATOR_OK)
    {
        status = bigint_set_string(limit, "99999999999999") == BIGINT_OK &&
            bigint_set_string(negative_limit, "-99999999999999") == BIGINT_OK ?
            CALCULATOR_OK : CALCULATOR_OUT_OF_MEMORY;
    }
    if (status == CALCULATOR_OK)
    {
        *candidate = bigint_compare(numerator, limit) <= 0 &&
            bigint_compare(numerator, negative_limit) >= 0 &&
            bigint_compare(denominator, limit) <= 0;
    }
    if (status == CALCULATOR_OK && *candidate && context->notation == CALCULATOR_NOTATION_AUTO)
    {
        status = bigint_set_string(limit, "10000") == BIGINT_OK ?
            CALCULATOR_OK : CALCULATOR_OUT_OF_MEMORY;
        if (status == CALCULATOR_OK)
        {
            *candidate = bigint_compare(denominator, limit) <= 0;
        }
    }
    bigint_destroy(numerator);
    bigint_destroy(denominator);
    bigint_destroy(limit);
    bigint_destroy(negative_limit);
    status = calculator_budget_status(status);
    if (owner)
    {
        numforge_budget_end();
    }
    return status;
}

CalculatorStatus calculator_format_value(
    const CalculatorValue *value, const CalculatorContext *context, char **result)
{
    BigDecimal *temporary;
    CalculatorStatus status;
    if (result != NULL)
    {
        *result = NULL;
    }
    if (value == NULL || context == NULL || result == NULL)
    {
        return CALCULATOR_NULL_ARGUMENT;
    }
    if (context->time_limit_ms < 0 || context->notation < CALCULATOR_NOTATION_AUTO ||
        context->notation > CALCULATOR_NOTATION_FRACTION)
    {
        return CALCULATOR_INVALID_ARGUMENT;
    }
    if (context->notation == CALCULATOR_NOTATION_FRACTION &&
        value->kind == CALCULATOR_VALUE_INTEGER)
    {
        return calculator_format_exact_text(value, context, result, NULL);
    }
    if (context->notation == CALCULATOR_NOTATION_FRACTION &&
        value->kind == CALCULATOR_VALUE_RATIONAL)
    {
        bool candidate = false;
        status = calculator_fraction_candidate(value->rational, context, &candidate);
        if (status != CALCULATOR_OK)
        {
            return status;
        }
        if (candidate)
        {
            status = calculator_format_exact_text(value, context, result, NULL);
            if (status != CALCULATOR_OK)
            {
                return status;
            }
            if (strlen(*result) <= 16U)
            {
                return CALCULATOR_OK;
            }
            free(*result);
            *result = NULL;
        }
    }
    if (value->kind == CALCULATOR_VALUE_DECIMAL)
    {
        return calculator_format_result(value->number, context, result);
    }
    temporary = bigdecimal_create();
    if (temporary == NULL)
    {
        return calculator_budget_status(CALCULATOR_OUT_OF_MEMORY);
    }
    status = calculator_materialize_exact(temporary, value, context);
    if (status == CALCULATOR_OK)
    {
        status = calculator_format_result(temporary, context, result);
    }
    bigdecimal_destroy(temporary);
    if (status == CALCULATOR_OK && context->notation == CALCULATOR_NOTATION_AUTO &&
        value->kind == CALCULATOR_VALUE_RATIONAL)
    {
        bool candidate = false;
        const char *exponent_marker = strchr(*result, 'E');
        status = calculator_fraction_candidate(value->rational, context, &candidate);
        if (status == CALCULATOR_OK && candidate && exponent_marker != NULL)
        {
            char *end;
            long long exponent;
            errno = 0;
            exponent = strtoll(exponent_marker + 1U, &end, 10);
            if (errno != 0 || *end != '\0' || exponent < 0 ||
                (unsigned long long)exponent + 5U > strlen(*result))
            {
                candidate = false;
            }
        }
        if (status == CALCULATOR_OK && candidate)
        {
            char *fraction = NULL;
            bool too_long = false;
            status = calculator_format_exact_text(value, context, &fraction, &too_long);
            if (too_long)
            {
                status = CALCULATOR_OK;
            }
            if (status == CALCULATOR_OK)
            {
                if (fraction != NULL && strlen(fraction) <= 16U &&
                    strlen(fraction) + 2U <= strlen(*result))
                {
                    free(*result);
                    *result = fraction;
                }
                else
                {
                    free(fraction);
                }
            }
        }
        if (status != CALCULATOR_OK)
        {
            free(*result);
            *result = NULL;
        }
    }
    return status;
}

bool calculator_value_matches(const CalculatorValue *value, const CalculatorContext *context)
{
    return value != NULL && value->number != NULL && context != NULL &&
           (value->kind == CALCULATOR_VALUE_DECIMAL ||
            value->context.significant_division == context->significant_division) &&
           value->context.angle_unit == context->angle_unit &&
           value->context.rounding == context->rounding &&
           (value->independent ||
            (value->context.division_scale == context->division_scale &&
             value->context.significant_division == context->significant_division));
}
