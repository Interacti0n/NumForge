#include "evaluator_internal.h"
#include "expression_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Trigonometric calls. The numeric library is radian-only; calculator angle
    mode performs guarded conversion at the client boundary. Explicit
    radians/degrees calls are independent of the selected mode.
------------------------------------------------------------------------------------------------------------------------------
*/

static CalculatorStatus calculator_convert_angle(
    BigDecimal *value,
    bool to_radians,
    int64_t digits
)
{
    BigDecimal *pi = bigdecimal_create();
    BigDecimal *factor = bigdecimal_create();
    BigDecimal *product = bigdecimal_create();
    BigDecimalStatus numeric_status = BIGDECIMAL_OUT_OF_MEMORY;
    int64_t conversion_digits;

    if (pi == NULL || factor == NULL || product == NULL)
    {
        goto cleanup;
    }

    if (digits > INT64_MAX - CALCULATOR_ANGLE_GUARD_DIGITS)
    {
        numeric_status = BIGDECIMAL_VALUE_TOO_LARGE;
        goto cleanup;
    }

    conversion_digits = digits + CALCULATOR_ANGLE_GUARD_DIGITS;
    numeric_status = bigdecimal_set_constant_significant(
        pi, BIGDECIMAL_CONSTANT_PI, conversion_digits, BIGDECIMAL_ROUND_HALF_EVEN);

    if (numeric_status != BIGDECIMAL_OK)
    {
        goto cleanup;
    }

    numeric_status = bigdecimal_set_string(factor, "180");

    if (numeric_status != BIGDECIMAL_OK)
    {
        goto cleanup;
    }

    numeric_status = to_radians ? bigdecimal_mul(product, value, pi)
                                : bigdecimal_mul(product, value, factor);

    if (numeric_status != BIGDECIMAL_OK)
    {
        goto cleanup;
    }

    numeric_status = bigdecimal_div_exact_or_significant(
        value,
        product,
        to_radians ? factor : pi,
        conversion_digits,
        BIGDECIMAL_ROUND_HALF_EVEN);

cleanup:
    bigdecimal_destroy(pi);
    bigdecimal_destroy(factor);
    bigdecimal_destroy(product);
    return calculator_from_bigdecimal_status(numeric_status);
}

/* Remove complete turns exactly before converting degrees to radians. */
static CalculatorStatus calculator_reduce_degrees(BigDecimal *value)
{
    BigDecimal *turn = bigdecimal_create();
    BigDecimal *count = bigdecimal_create();
    BigDecimal *product = bigdecimal_create();
    BigDecimalStatus status = BIGDECIMAL_OUT_OF_MEMORY;

    if (turn != NULL && count != NULL && product != NULL)
    {
        status = bigdecimal_set_string(turn, "360");
        if (status == BIGDECIMAL_OK)
        {
            status = bigdecimal_div(count, value, turn, 0, BIGDECIMAL_ROUND_HALF_EVEN);
        }
        if (status == BIGDECIMAL_OK)
        {
            status = bigdecimal_mul(product, count, turn);
        }
        if (status == BIGDECIMAL_OK)
        {
            status = bigdecimal_sub(value, value, product);
        }
    }

    bigdecimal_destroy(turn);
    bigdecimal_destroy(count);
    bigdecimal_destroy(product);
    return calculator_from_bigdecimal_status(status);
}

static CalculatorStatus calculator_degree_tangent_pole(
    const BigDecimal *value,
    bool *is_pole
)
{
    BigInt *integer = bigint_create();
    BigInt *period = bigint_create();
    BigInt *remainder = bigint_create();
    BigInt *absolute = bigint_create();
    BigInt *right_angle = bigint_create();
    BigDecimalStatus decimal_status;
    BigIntStatus integer_status = BIGINT_OUT_OF_MEMORY;
    int comparison = 0;

    *is_pole = false;

    if (integer == NULL || period == NULL || remainder == NULL ||
        absolute == NULL || right_angle == NULL)
    {
        goto cleanup;
    }

    decimal_status = bigdecimal_to_bigint(integer, value);

    if (decimal_status == BIGDECIMAL_INVALID_ARGUMENT)
    {
        integer_status = BIGINT_OK;
        goto cleanup;
    }

    if (decimal_status != BIGDECIMAL_OK)
    {
        bigint_destroy(integer);
        bigint_destroy(period);
        bigint_destroy(remainder);
        bigint_destroy(absolute);
        bigint_destroy(right_angle);
        return calculator_from_bigdecimal_status(decimal_status);
    }

    integer_status = bigint_set_string(period, "180");

    if (integer_status == BIGINT_OK)
    {
        integer_status = bigint_set_string(right_angle, "90");
    }

    if (integer_status == BIGINT_OK)
    {
        integer_status = bigint_mod(remainder, integer, period);
    }

    if (integer_status == BIGINT_OK)
    {
        integer_status = bigint_abs(absolute, remainder);
    }

    if (integer_status == BIGINT_OK)
    {
        comparison = bigint_compare(absolute, right_angle);
        *is_pole = comparison == 0;
    }

cleanup:
    bigint_destroy(integer);
    bigint_destroy(period);
    bigint_destroy(remainder);
    bigint_destroy(absolute);
    bigint_destroy(right_angle);
    return calculator_from_bigint_status(integer_status);
}

static CalculatorStatus calculator_angle_integer_digits(
    const BigDecimal *value,
    int64_t *digits
)
{
    char *text = NULL;
    const char *cursor;
    const char *exponent;
    BigDecimalStatus numeric_status;
    int64_t count = 0;

    numeric_status = bigdecimal_format(
        value, 0, BIGDECIMAL_ROUND_TOWARD_ZERO, &text);

    if (numeric_status != BIGDECIMAL_OK)
    {
        return calculator_from_bigdecimal_status(numeric_status);
    }

    exponent = strchr(text, 'E');

    if (exponent != NULL)
    {
        bool positive = exponent[1] == '+';

        cursor = exponent + 2;

        while (*cursor >= '0' && *cursor <= '9')
        {
            int digit = *cursor - '0';

            if (count > (INT64_MAX - digit) / 10)
            {
                free(text);
                return CALCULATOR_VALUE_TOO_LARGE;
            }

            count = count * 10 + digit;
            cursor++;
        }

        if (positive)
        {
            if (count == INT64_MAX)
            {
                free(text);
                return CALCULATOR_VALUE_TOO_LARGE;
            }

            count++;
        }
        else
        {
            count = 0;
        }
    }
    else
    {
        cursor = text;

        if (*cursor == '-')
        {
            cursor++;
        }

        if (*cursor != '0')
        {
            while (*cursor >= '0' && *cursor <= '9')
            {
                count++;
                cursor++;
            }
        }
    }

    free(text);
    *digits = count;
    return CALCULATOR_OK;
}

static CalculatorStatus calculator_refine_angle_argument(
    BigDecimal **value,
    const CalculatorExpression *argument,
    const CalculatorEvaluation *evaluation,
    int64_t result_digits,
    CalculatorError *error
)
{
    CalculatorContext refined_context;
    CalculatorEvaluation refined_evaluation;
    BigDecimal *refined_value = NULL;
    CalculatorStatus status;
    int64_t integer_digits = 0;
    int64_t required_digits;
    int64_t guard_digits = CALCULATOR_ANGLE_GUARD_DIGITS +
                           CALCULATOR_ANGLE_REDUCTION_GUARD_DIGITS;

    status = calculator_angle_integer_digits(*value, &integer_digits);

    if (status != CALCULATOR_OK)
    {
        return status;
    }

    if (result_digits > INT64_MAX - guard_digits ||
        result_digits + guard_digits > INT64_MAX - integer_digits)
    {
        return CALCULATOR_VALUE_TOO_LARGE;
    }

    required_digits = result_digits + guard_digits + integer_digits;

    if (required_digits <= evaluation->context->division_scale)
    {
        return CALCULATOR_OK;
    }

    refined_context = *evaluation->context;
    refined_context.division_scale = required_digits;
    refined_evaluation = *evaluation;
    refined_evaluation.context = &refined_context;

    status = calculator_evaluate_expression(
        &refined_value, argument, &refined_evaluation, error);

    if (status != CALCULATOR_OK)
    {
        return status;
    }

    bigdecimal_destroy(*value);
    *value = refined_value;
    return CALCULATOR_OK;
}

CalculatorStatus calculator_evaluate_trigonometric_call(
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

    if (status == CALCULATOR_OK && operation == CALCULATOR_FUNCTION_RADIANS)
    {
        status = calculator_convert_angle(value, true, digits);
    }
    else if (status == CALCULATOR_OK && operation == CALCULATOR_FUNCTION_DEGREES)
    {
        status = calculator_convert_angle(value, false, digits);
    }
    else if (status == CALCULATOR_OK)
    {
        bool inverse = operation == CALCULATOR_FUNCTION_ASIN ||
                       operation == CALCULATOR_FUNCTION_ACOS ||
                       operation == CALCULATOR_FUNCTION_ATAN;

        if (!inverse)
        {
            status = calculator_refine_angle_argument(
                &value,
                expression->data.call.arguments[0],
                evaluation,
                digits,
                error);
        }

        if (status == CALCULATOR_OK && !inverse &&
            evaluation->context->angle_unit == CALCULATOR_ANGLE_DEGREES)
        {
            if (operation == CALCULATOR_FUNCTION_TAN)
            {
                bool is_pole = false;
                status = calculator_degree_tangent_pole(value, &is_pole);

                if (status == CALCULATOR_OK && is_pole)
                {
                    status = CALCULATOR_INVALID_ARGUMENT;
                }
            }

            if (status == CALCULATOR_OK)
            {
                status = calculator_reduce_degrees(value);
            }

            if (status == CALCULATOR_OK)
            {
                status = calculator_convert_angle(
                    value, true, digits + CALCULATOR_ANGLE_REDUCTION_GUARD_DIGITS + 1);
            }
        }

        if (status == CALCULATOR_OK)
        {
            BigDecimalStatus numeric_status;

            switch (operation)
            {
                case CALCULATOR_FUNCTION_SIN:
                    numeric_status = bigdecimal_sin(
                        value, value, digits, evaluation->context->rounding);
                    break;
                case CALCULATOR_FUNCTION_COS:
                    numeric_status = bigdecimal_cos(
                        value, value, digits, evaluation->context->rounding);
                    break;
                case CALCULATOR_FUNCTION_TAN:
                    numeric_status = bigdecimal_tan(
                        value, value, digits, evaluation->context->rounding);
                    break;
                case CALCULATOR_FUNCTION_ASIN:
                    numeric_status = bigdecimal_asin(
                        value, value, digits, evaluation->context->rounding);
                    break;
                case CALCULATOR_FUNCTION_ACOS:
                    numeric_status = bigdecimal_acos(
                        value, value, digits, evaluation->context->rounding);
                    break;
                default:
                    numeric_status = bigdecimal_atan(
                        value, value, digits, evaluation->context->rounding);
                    break;
            }

            status = calculator_from_bigdecimal_status(numeric_status);
        }

        if (status == CALCULATOR_OK && inverse &&
            evaluation->context->angle_unit == CALCULATOR_ANGLE_DEGREES)
        {
            status = calculator_convert_angle(value, false, digits);
        }
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
