#include "evaluator_internal.h"
#include "expression_internal.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#define CALCULATOR_STRINGIFY_VALUE(value) #value
#define CALCULATOR_STRINGIFY(value) CALCULATOR_STRINGIFY_VALUE(value)

/* Named decimal functions other than trigonometric operations. */

CalculatorStatus calculator_evaluate_integer_call(
    BigDecimal **result,
    const CalculatorExpression *expression,
    const CalculatorEvaluation *evaluation,
    CalculatorError *error
)
{
    BigInt *arguments[2] = {NULL, NULL};
    BigInt *integer_result = NULL;
    BigDecimal *value = NULL;
    CalculatorStatus status = CALCULATOR_OK;
    CalculatorFunctionImplementation operation = expression->data.call.function->implementation;
    bool child_error = false;

    for (size_t i = 0; i < expression->data.call.count; i++)
    {
        status =
            calculator_evaluate_expression(&value, expression->data.call.arguments[i], evaluation, error);

        if (status != CALCULATOR_OK)
        {
            child_error = true;
            break;
        }

        {
            bool signed_input = operation == CALCULATOR_FUNCTION_GCD ||
                                operation == CALCULATOR_FUNCTION_LCM ||
                                operation == CALCULATOR_FUNCTION_MOD;
            status = calculator_bigdecimal_to_bigint(
                &arguments[i], value, signed_input);
        }
        bigdecimal_destroy(value);
        value = NULL;

        if (status != CALCULATOR_OK)
        {
            break;
        }
    }

    if (status == CALCULATOR_OK)
    {
        integer_result = bigint_create();

        if (integer_result == NULL)
        {
            status = CALCULATOR_OUT_OF_MEMORY;
        }
    }

    if (status == CALCULATOR_OK)
    {
        BigIntStatus integer_status;

        switch (operation)
        {
            case CALCULATOR_FUNCTION_GCD:
                integer_status = bigint_gcd(integer_result, arguments[0], arguments[1]);
                break;
            case CALCULATOR_FUNCTION_LCM:
                integer_status = bigint_lcm(integer_result, arguments[0], arguments[1]);
                break;
            case CALCULATOR_FUNCTION_MOD:
                integer_status = bigint_mod(integer_result, arguments[0], arguments[1]);
                break;
            case CALCULATOR_FUNCTION_NPR:
                integer_status = bigint_permutation(
                    integer_result, arguments[0], arguments[1]);
                break;
            case CALCULATOR_FUNCTION_NCR:
                integer_status = bigint_combination(
                    integer_result, arguments[0], arguments[1]);
                break;
            default:
                integer_status = bigint_isqrt(integer_result, arguments[0]);
                break;
        }

        status = calculator_from_bigint_status(integer_status);
    }

    if (status == CALCULATOR_OK)
    {
        value = bigdecimal_create();
        status = value == NULL ? CALCULATOR_OUT_OF_MEMORY
                               : calculator_set_bigdecimal_from_bigint(value, integer_result);
    }

    bigint_destroy(arguments[0]);
    bigint_destroy(arguments[1]);
    bigint_destroy(integer_result);

    if (calculator_time_limit_reached(evaluation))
    {
        status = CALCULATOR_TIME_LIMIT;
    }

    if (status != CALCULATOR_OK)
    {
        bigdecimal_destroy(value);

        if (!child_error)
        {
            calculator_error_set(error, status, expression->offset);
        }

        return status;
    }

    *result = value;

    return CALCULATOR_OK;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Real-root calls and application degree limits.
------------------------------------------------------------------------------------------------------------------------------
*/

static bool calculator_decimal_zero(
    const BigDecimal *value
)
{
    bool zero = false;
    (void)bigdecimal_is_zero(&zero, value);

    return zero;
}

/* Root degree is an application parameter, not a rounded numeric argument. */

CalculatorStatus calculator_evaluate_root_call(
    BigDecimal **result,
    const CalculatorExpression *expression,
    const CalculatorEvaluation *evaluation,
    CalculatorError *error
)
{
    BigDecimal *value = NULL, *degree_value = NULL, *limit = NULL;
    CalculatorFunctionImplementation operation = expression->data.call.function->implementation;
    uint32_t degree = operation == CALCULATOR_FUNCTION_CBRT ? 3U : 2U;
    CalculatorStatus status =
        calculator_evaluate_expression(&value, expression->data.call.arguments[0], evaluation, error);
    bool child_error = status != CALCULATOR_OK;

    if (status == CALCULATOR_OK && operation == CALCULATOR_FUNCTION_ROOT)
    {
        status = calculator_evaluate_expression(
            &degree_value, expression->data.call.arguments[1], evaluation, error);
        child_error = status != CALCULATOR_OK;

        if (status == CALCULATOR_OK &&
            (!calculator_nonnegative_integer(degree_value) || calculator_decimal_zero(degree_value)))
        {
            status = CALCULATOR_INVALID_ARGUMENT;
        }

        if (status == CALCULATOR_OK)
        {
            limit = bigdecimal_create();
            status = limit == NULL ? CALCULATOR_OUT_OF_MEMORY
                                   : calculator_from_bigdecimal_status(bigdecimal_set_string(
                                         limit, CALCULATOR_STRINGIFY(CALCULATOR_MAX_ROOT_DEGREE)));
        }

        if (status == CALCULATOR_OK)
        {
            int comparison = 0;
            status = calculator_from_bigdecimal_status(bigdecimal_compare(&comparison, degree_value, limit));

            if (status == CALCULATOR_OK && comparison > 0)
            {
                status = CALCULATOR_VALUE_TOO_LARGE;
            }
        }

        if (status == CALCULATOR_OK)
        {
            char *text = NULL;
            status = calculator_from_bigdecimal_status(bigdecimal_to_string(degree_value, &text));

            if (status == CALCULATOR_OK)
            {
                degree = (uint32_t)strtoul(text, NULL, 10);
            }

            free(text);
        }
    }

    if (status == CALCULATOR_OK)
    {
        int64_t digits = evaluation->context->division_scale;

        if (digits < CALCULATOR_DEFAULT_DIVISION_SCALE)
        {
            digits = CALCULATOR_DEFAULT_DIVISION_SCALE;
        }

        status = calculator_from_bigdecimal_status(
            bigdecimal_root(value, value, degree, digits, evaluation->context->rounding));
    }

    bigdecimal_destroy(degree_value);
    bigdecimal_destroy(limit);

    if (status != CALCULATOR_OK)
    {
        bigdecimal_destroy(value);

        if (!child_error)
        {
            calculator_error_set(error, status, expression->offset);
        }

        return status;
    }

    *result = value;

    return CALCULATOR_OK;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Exponential and logarithmic calls. log(x) uses base ten; log(x; base)
    accepts an explicit positive base other than one.
------------------------------------------------------------------------------------------------------------------------------
*/

CalculatorStatus calculator_evaluate_transcendental_call(
    BigDecimal **result,
    const CalculatorExpression *expression,
    const CalculatorEvaluation *evaluation,
    CalculatorError *error
)
{
    BigDecimal *value = NULL;
    BigDecimal *base = NULL;
    CalculatorFunctionImplementation operation = expression->data.call.function->implementation;
    CalculatorStatus status =
        calculator_evaluate_expression(&value, expression->data.call.arguments[0], evaluation, error);
    bool child_error = status != CALCULATOR_OK;
    int64_t digits = evaluation->context->division_scale;

    if (status == CALCULATOR_OK && expression->data.call.count == 2U)
    {
        status = calculator_evaluate_expression(
            &base, expression->data.call.arguments[1], evaluation, error);
        child_error = status != CALCULATOR_OK;
    }

    if (digits < CALCULATOR_DEFAULT_DIVISION_SCALE)
    {
        digits = CALCULATOR_DEFAULT_DIVISION_SCALE;
    }

    if (status == CALCULATOR_OK && operation == CALCULATOR_FUNCTION_EXP)
    {
        status = calculator_from_bigdecimal_status(
            bigdecimal_exp(value, value, digits, evaluation->context->rounding));
    }
    else if (status == CALCULATOR_OK && operation == CALCULATOR_FUNCTION_LN)
    {
        status = calculator_from_bigdecimal_status(
            bigdecimal_ln(value, value, digits, evaluation->context->rounding));
    }
    else if (status == CALCULATOR_OK && base == NULL)
    {
        status = calculator_from_bigdecimal_status(
            bigdecimal_log10(value, value, digits, evaluation->context->rounding));
    }
    else if (status == CALCULATOR_OK)
    {
        status = calculator_from_bigdecimal_status(
            bigdecimal_log(value, value, base, digits, evaluation->context->rounding));
    }

    bigdecimal_destroy(base);

    if (status != CALCULATOR_OK)
    {
        bigdecimal_destroy(value);

        if (!child_error)
        {
            calculator_error_set(error, status, expression->offset);
        }

        return status;
    }

    *result = value;
    return CALCULATOR_OK;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Hyperbolic calls use the active numeric precision but are independent of
    the calculator's RAD/DEG angle mode.
------------------------------------------------------------------------------------------------------------------------------
*/

CalculatorStatus calculator_evaluate_hyperbolic_call(
    BigDecimal **result,
    const CalculatorExpression *expression,
    const CalculatorEvaluation *evaluation,
    CalculatorError *error
)
{
    CalculatorFunctionImplementation operation = expression->data.call.function->implementation;
    BigDecimal *value = NULL;
    CalculatorStatus status =
        calculator_evaluate_expression(&value, expression->data.call.arguments[0], evaluation, error);
    bool child_error = status != CALCULATOR_OK;
    int64_t digits = evaluation->context->division_scale;

    if (digits < CALCULATOR_DEFAULT_DIVISION_SCALE)
    {
        digits = CALCULATOR_DEFAULT_DIVISION_SCALE;
    }

    if (status == CALCULATOR_OK)
    {
        BigDecimalStatus numeric_status;

        switch (operation)
        {
            case CALCULATOR_FUNCTION_SINH:
                numeric_status = bigdecimal_sinh(
                    value, value, digits, evaluation->context->rounding);
                break;
            case CALCULATOR_FUNCTION_COSH:
                numeric_status = bigdecimal_cosh(
                    value, value, digits, evaluation->context->rounding);
                break;
            case CALCULATOR_FUNCTION_TANH:
                numeric_status = bigdecimal_tanh(
                    value, value, digits, evaluation->context->rounding);
                break;
            case CALCULATOR_FUNCTION_ASINH:
                numeric_status = bigdecimal_asinh(
                    value, value, digits, evaluation->context->rounding);
                break;
            case CALCULATOR_FUNCTION_ACOSH:
                numeric_status = bigdecimal_acosh(
                    value, value, digits, evaluation->context->rounding);
                break;
            default:
                numeric_status = bigdecimal_atanh(
                    value, value, digits, evaluation->context->rounding);
                break;
        }

        status = calculator_from_bigdecimal_status(numeric_status);
    }

    if (status == CALCULATOR_OK && calculator_time_limit_reached(evaluation))
    {
        status = CALCULATOR_TIME_LIMIT;
    }

    if (status != CALCULATOR_OK)
    {
        bigdecimal_destroy(value);

        if (!child_error)
        {
            calculator_error_set(error, status, expression->offset);
        }

        return status;
    }

    *result = value;
    return CALCULATOR_OK;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Decimal selection calls: absolute value, sign, minimum, and maximum.
------------------------------------------------------------------------------------------------------------------------------
*/

/* Evaluate basic calls left to right, retaining at most the selected value
 * and the current argument. Even an unselected argument must be evaluated
 * so that its errors are not silently discarded. */

CalculatorStatus calculator_evaluate_basic_call(
    BigDecimal **result,
    const CalculatorExpression *expression,
    const CalculatorEvaluation *evaluation,
    CalculatorError *error
)
{
    BigDecimal *selected = NULL;
    CalculatorFunctionImplementation operation = expression->data.call.function->implementation;
    CalculatorStatus status =
        calculator_evaluate_expression(&selected, expression->data.call.arguments[0], evaluation, error);

    if (status != CALCULATOR_OK)
    {
        return status;
    }

    if (operation == CALCULATOR_FUNCTION_ABS)
    {
        status = calculator_from_bigdecimal_status(bigdecimal_abs(selected, selected));
    }
    else if (operation == CALCULATOR_FUNCTION_SIGN)
    {
        int numeric_sign = 0;
        (void)bigdecimal_sign(&numeric_sign, selected);
        const char *sign = numeric_sign == 0 ? "0" : numeric_sign < 0 ? "-1" : "1";
        status = calculator_from_bigdecimal_status(bigdecimal_set_string(selected, sign));
    }
    else
    {
        for (size_t index = 1; index < expression->data.call.count; index++)
        {
            BigDecimal *argument = NULL;
            int comparison = 0;
            status = calculator_evaluate_expression(
                &argument, expression->data.call.arguments[index], evaluation, error);

            if (status != CALCULATOR_OK)
            {
                bigdecimal_destroy(selected);

                return status;
            }

            status = calculator_from_bigdecimal_status(bigdecimal_compare(&comparison, argument, selected));

            if (status == CALCULATOR_OK && ((operation == CALCULATOR_FUNCTION_MIN && comparison < 0) ||
                                            (operation == CALCULATOR_FUNCTION_MAX && comparison > 0)))
            {
                BigDecimal *previous = selected;
                selected = argument;
                argument = previous;
            }

            bigdecimal_destroy(argument);

            if (status != CALCULATOR_OK)
            {
                break;
            }
        }
    }

    if (status == CALCULATOR_OK && calculator_time_limit_reached(evaluation))
    {
        status = CALCULATOR_TIME_LIMIT;
    }

    if (status != CALCULATOR_OK)
    {
        bigdecimal_destroy(selected);
        calculator_error_set(error, status, expression->offset);

        return status;
    }

    *result = selected;

    return CALCULATOR_OK;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Variadic decimal aggregates and statistics. The public BigDecimal API owns
    all numeric semantics; the calculator only evaluates arguments and supplies
    context.
------------------------------------------------------------------------------------------------------------------------------
*/

CalculatorStatus calculator_evaluate_aggregate_call(
    BigDecimal **result,
    const CalculatorExpression *expression,
    const CalculatorEvaluation *evaluation,
    CalculatorError *error
)
{
    BigDecimal *arguments[CALCULATOR_MAX_CALL_ARGUMENTS] = {NULL};
    const BigDecimal *inputs[CALCULATOR_MAX_CALL_ARGUMENTS];
    BigDecimal *value = NULL;
    CalculatorStatus status = CALCULATOR_OK;
    CalculatorFunctionImplementation operation = expression->data.call.function->implementation;
    bool child_error = false;

    for (size_t index = 0U; index < expression->data.call.count; index++)
    {
        status = calculator_evaluate_expression(
            &arguments[index], expression->data.call.arguments[index], evaluation, error);

        if (status != CALCULATOR_OK)
        {
            child_error = true;
            break;
        }

        inputs[index] = arguments[index];
    }

    if (status == CALCULATOR_OK)
    {
        value = bigdecimal_create();

        if (value == NULL)
        {
            status = CALCULATOR_OUT_OF_MEMORY;
        }
    }

    if (status == CALCULATOR_OK)
    {
        BigDecimalStatus decimal_status;

        if (operation == CALCULATOR_FUNCTION_SUM)
        {
            decimal_status = bigdecimal_sum(value, inputs, expression->data.call.count);
        }
        else if (operation == CALCULATOR_FUNCTION_PRODUCT)
        {
            decimal_status = bigdecimal_product(value, inputs, expression->data.call.count);
        }
        else if (operation == CALCULATOR_FUNCTION_MEAN)
        {
            decimal_status = bigdecimal_mean(
                value,
                inputs,
                expression->data.call.count,
                evaluation->context->division_scale,
                evaluation->context->rounding);
        }
        else if (operation == CALCULATOR_FUNCTION_MEDIAN)
        {
            decimal_status = bigdecimal_median(
                value, inputs, expression->data.call.count);
        }
        else if (operation == CALCULATOR_FUNCTION_GEOMETRIC_MEAN)
        {
            decimal_status = bigdecimal_geometric_mean(
                value,
                inputs,
                expression->data.call.count,
                evaluation->context->division_scale,
                evaluation->context->rounding);
        }
        else if (operation == CALCULATOR_FUNCTION_HARMONIC_MEAN)
        {
            decimal_status = bigdecimal_harmonic_mean(
                value,
                inputs,
                expression->data.call.count,
                evaluation->context->division_scale,
                evaluation->context->rounding);
        }
        else if (operation == CALCULATOR_FUNCTION_VARIANCE)
        {
            decimal_status = bigdecimal_variance_population(
                value,
                inputs,
                expression->data.call.count,
                evaluation->context->division_scale,
                evaluation->context->rounding);
        }
        else if (operation == CALCULATOR_FUNCTION_STANDARD_DEVIATION_POPULATION)
        {
            decimal_status = bigdecimal_standard_deviation_population(
                value,
                inputs,
                expression->data.call.count,
                evaluation->context->division_scale,
                evaluation->context->rounding);
        }
        else
        {
            decimal_status = bigdecimal_standard_deviation_sample(
                value,
                inputs,
                expression->data.call.count,
                evaluation->context->division_scale,
                evaluation->context->rounding);
        }

        status = calculator_from_bigdecimal_status(decimal_status);
    }

    for (size_t index = 0U; index < expression->data.call.count; index++)
    {
        bigdecimal_destroy(arguments[index]);
    }

    if (status == CALCULATOR_OK && calculator_time_limit_reached(evaluation))
    {
        status = CALCULATOR_TIME_LIMIT;
    }

    if (status != CALCULATOR_OK)
    {
        bigdecimal_destroy(value);

        if (!child_error)
        {
            calculator_error_set(error, status, expression->offset);
        }

        return status;
    }

    *result = value;
    return CALCULATOR_OK;
}

/* Decimal-place arguments are exact integers. Bounds are checked before text
 * conversion so compact values with enormous exponents are rejected without
 * expanding them into impractically large strings. */
static CalculatorStatus calculator_decimal_to_i64(
    const BigDecimal *value,
    int64_t *result
)
{
    BigDecimal *minimum = NULL;
    BigDecimal *maximum = NULL;
    char *text = NULL;
    bool integer = false;
    int comparison = 0;
    CalculatorStatus status = calculator_from_bigdecimal_status(
        bigdecimal_is_integer(&integer, value));

    if (status != CALCULATOR_OK || !integer)
    {
        return status == CALCULATOR_OK ? CALCULATOR_INVALID_ARGUMENT : status;
    }

    minimum = bigdecimal_create();
    maximum = bigdecimal_create();

    if (minimum == NULL || maximum == NULL)
    {
        status = CALCULATOR_OUT_OF_MEMORY;
        goto cleanup;
    }

    status = calculator_from_bigdecimal_status(
        bigdecimal_set_string(minimum, "-9223372036854775808"));

    if (status == CALCULATOR_OK)
    {
        status = calculator_from_bigdecimal_status(
            bigdecimal_set_string(maximum, "9223372036854775807"));
    }

    if (status == CALCULATOR_OK)
    {
        status = calculator_from_bigdecimal_status(
            bigdecimal_compare(&comparison, value, minimum));
    }

    if (status == CALCULATOR_OK && comparison < 0)
    {
        status = CALCULATOR_VALUE_TOO_LARGE;
    }

    if (status == CALCULATOR_OK)
    {
        status = calculator_from_bigdecimal_status(
            bigdecimal_compare(&comparison, value, maximum));
    }

    if (status == CALCULATOR_OK && comparison > 0)
    {
        status = CALCULATOR_VALUE_TOO_LARGE;
    }

    if (status == CALCULATOR_OK)
    {
        status = calculator_from_bigdecimal_status(
            bigdecimal_to_string(value, &text));
    }

    if (status == CALCULATOR_OK)
    {
        *result = (int64_t)strtoimax(text, NULL, 10);
    }

cleanup:
    free(text);
    bigdecimal_destroy(minimum);
    bigdecimal_destroy(maximum);
    return status;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Integer and decimal-place rounding calls.
------------------------------------------------------------------------------------------------------------------------------
*/

CalculatorStatus calculator_evaluate_rounding_call(
    BigDecimal **result,
    const CalculatorExpression *expression,
    const CalculatorEvaluation *evaluation,
    CalculatorError *error
)
{
    CalculatorFunctionImplementation operation = expression->data.call.function->implementation;
    BigDecimal *value = NULL;
    BigDecimal *places_value = NULL;
    int64_t places = 0;
    CalculatorStatus status = calculator_evaluate_expression(
        &value, expression->data.call.arguments[0], evaluation, error);
    bool child_error = status != CALCULATOR_OK;

    if (status == CALCULATOR_OK && expression->data.call.count == 2U)
    {
        status = calculator_evaluate_expression(
            &places_value, expression->data.call.arguments[1], evaluation, error);
        child_error = status != CALCULATOR_OK;
    }

    if (status == CALCULATOR_OK && places_value != NULL)
    {
        status = calculator_decimal_to_i64(places_value, &places);
    }

    if (status == CALCULATOR_OK)
    {
        BigDecimalStatus decimal_status;

        switch (operation)
        {
            case CALCULATOR_FUNCTION_FLOOR:
                decimal_status = bigdecimal_floor(value, value);
                break;
            case CALCULATOR_FUNCTION_CEIL:
                decimal_status = bigdecimal_ceil(value, value);
                break;
            case CALCULATOR_FUNCTION_TRUNC:
                decimal_status = bigdecimal_trunc(value, value);
                break;
            default:
                decimal_status = bigdecimal_round(value, value, places);
                break;
        }

        status = calculator_from_bigdecimal_status(decimal_status);
    }

    bigdecimal_destroy(places_value);

    if (status == CALCULATOR_OK && calculator_time_limit_reached(evaluation))
    {
        status = CALCULATOR_TIME_LIMIT;
    }

    if (status != CALCULATOR_OK)
    {
        bigdecimal_destroy(value);

        if (!child_error)
        {
            calculator_error_set(error, status, expression->offset);
        }

        return status;
    }

    *result = value;
    return CALCULATOR_OK;
}
