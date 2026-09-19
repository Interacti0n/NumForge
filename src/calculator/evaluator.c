#include "evaluator.h"
#include "expression_internal.h"

#include <numforge/bigint.h>
#include <numforge/runtime.h>

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#define CALCULATOR_STRINGIFY_VALUE(value) #value
#define CALCULATOR_STRINGIFY(value) CALCULATOR_STRINGIFY_VALUE(value)

/*
------------------------------------------------------------------------------------------------------------------------------
    Evaluator implementation. It recursively evaluates AST nodes into temporary
    BigDecimal values, then copies a final successful value to the caller's
    destination. The caller's result therefore remains unchanged on failure.
------------------------------------------------------------------------------------------------------------------------------
*/

/*
------------------------------------------------------------------------------------------------------------------------------
    Internal helper functions for evaluator operations.
------------------------------------------------------------------------------------------------------------------------------
*/

typedef struct CalculatorConstantCache
{
    BigDecimal *values[CALCULATOR_CONSTANT_COUNT];
    int64_t digits[CALCULATOR_CONSTANT_COUNT];
} CalculatorConstantCache;

typedef struct CalculatorEvaluation
{
    const CalculatorContext *context;
    CalculatorConstantCache *constant_cache;
} CalculatorEvaluation;

static bool calculator_time_limit_reached(
    const CalculatorEvaluation *evaluation
)
{
    (void)evaluation;

    return !numforge_budget_check() && numforge_budget_failure() == NUMFORGE_BUDGET_TIME;
}

static CalculatorStatus calculator_evaluate_expression(
    BigDecimal **result,
    const CalculatorExpression *expression,
    const CalculatorEvaluation *evaluation,
    CalculatorError *error
);

static CalculatorStatus calculator_from_bigdecimal_status(
    BigDecimalStatus status
)
{
    switch (status)
    {
        case BIGDECIMAL_OK:
            return CALCULATOR_OK;
        case BIGDECIMAL_NULL_ARGUMENT:
            return CALCULATOR_NULL_ARGUMENT;
        case BIGDECIMAL_OUT_OF_MEMORY:
            return CALCULATOR_OUT_OF_MEMORY;
        case BIGDECIMAL_DIVISION_BY_ZERO:
            return CALCULATOR_DIVISION_BY_ZERO;
        case BIGDECIMAL_VALUE_TOO_LARGE:
            return CALCULATOR_VALUE_TOO_LARGE;
        case BIGDECIMAL_SCALE_OVERFLOW:
            return CALCULATOR_SCALE_OVERFLOW;
        default:
            return CALCULATOR_INVALID_ARGUMENT;
    }
}

static CalculatorStatus calculator_from_bigint_status(
    BigIntStatus status
)
{
    switch (status)
    {
        case BIGINT_OK:
            return CALCULATOR_OK;
        case BIGINT_NULL_ARGUMENT:
            return CALCULATOR_NULL_ARGUMENT;
        case BIGINT_OUT_OF_MEMORY:
            return CALCULATOR_OUT_OF_MEMORY;
        case BIGINT_VALUE_TOO_LARGE:
            return CALCULATOR_VALUE_TOO_LARGE;
        case BIGINT_DIVISION_BY_ZERO:
            return CALCULATOR_DIVISION_BY_ZERO;
        case BIGINT_NEGATIVE_ARGUMENT:
        case BIGINT_INVALID_ARGUMENT:
        default:
            return CALCULATOR_INVALID_ARGUMENT;
    }
}

static bool calculator_valid_rounding(
    BigDecimalRoundingMode rounding
)
{
    return rounding >= BIGDECIMAL_ROUND_TOWARD_ZERO && rounding <= BIGDECIMAL_ROUND_HALF_EVEN;
}

static CalculatorStatus calculator_set_number(
    BigDecimal *value,
    const char *text
)
{
    BigDecimalStatus decimal_status;
    const char *separator = strchr(text, ',');

    if (separator == NULL)
    {
        return calculator_from_bigdecimal_status(bigdecimal_set_string(value, text));
    }

    {
        size_t length = strlen(text);
        char *normalized = numforge_malloc(length + 1U);

        if (normalized == NULL)
        {
            return CALCULATOR_OUT_OF_MEMORY;
        }

        memcpy(normalized, text, length + 1U);
        normalized[separator - text] = '.';
        decimal_status = bigdecimal_set_string(value, normalized);
        free(normalized);
    }

    return calculator_from_bigdecimal_status(decimal_status);
}

/* Domain checks use public predicates, never numeric representation fields. */

static bool calculator_nonnegative_integer(
    const BigDecimal *value
)
{
    bool integer = false;
    int sign = 0;

    return bigdecimal_is_integer(&integer, value) == BIGDECIMAL_OK &&
           bigdecimal_sign(&sign, value) == BIGDECIMAL_OK && integer && sign >= 0;
}

static CalculatorStatus calculator_bigdecimal_to_bigint(
    BigInt **result,
    const BigDecimal *value,
    bool signed_input
)
{
    if (!signed_input && !calculator_nonnegative_integer(value))
    {
        return CALCULATOR_INVALID_ARGUMENT;
    }

    BigInt *integer = bigint_create();

    if (integer == NULL)
    {
        return CALCULATOR_OUT_OF_MEMORY;
    }

    CalculatorStatus status = calculator_from_bigdecimal_status(bigdecimal_to_bigint(integer, value));

    if (status != CALCULATOR_OK)
    {
        bigint_destroy(integer);
    }
    else
    {
        *result = integer;
    }

    return status;
}

static CalculatorStatus calculator_set_bigdecimal_from_bigint(
    BigDecimal *value,
    const BigInt *integer
)
{
    return calculator_from_bigdecimal_status(bigdecimal_from_bigint(value, integer));
}

static CalculatorStatus calculator_check_factorial_limit(
    const BigDecimal *value
)
{
    BigDecimal *limit;
    CalculatorStatus status;
    int comparison = 0;

    if (!calculator_nonnegative_integer(value))
    {
        return CALCULATOR_INVALID_ARGUMENT;
    }

    limit = bigdecimal_create();

    if (limit == NULL)
    {
        return CALCULATOR_OUT_OF_MEMORY;
    }

    status = calculator_from_bigdecimal_status(
        bigdecimal_set_string(limit, CALCULATOR_STRINGIFY(CALCULATOR_FACTORIAL_MAX_N)));

    if (status == CALCULATOR_OK)
    {
        status = calculator_from_bigdecimal_status(bigdecimal_compare(&comparison, value, limit));
    }

    if (status == CALCULATOR_OK && comparison > 0)
    {
        status = CALCULATOR_VALUE_TOO_LARGE;
    }

    bigdecimal_destroy(limit);

    return status;
}

static CalculatorStatus calculator_bigdecimal_pow(
    BigDecimal *result,
    const BigDecimal *base,
    const BigDecimal *exponent,
    const CalculatorEvaluation *evaluation
)
{
    (void)evaluation;
    BigInt *integer = NULL;
    CalculatorStatus status = calculator_bigdecimal_to_bigint(&integer, exponent, false);

    if (status == CALCULATOR_OK)
    {
        status = calculator_from_bigdecimal_status(bigdecimal_pow(result, base, integer));
    }

    bigint_destroy(integer);

    return status;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Postfix operations: square, cube, and factorial.
------------------------------------------------------------------------------------------------------------------------------
*/

static CalculatorStatus calculator_evaluate_postfix(
    BigDecimal **result,
    const CalculatorExpression *expression,
    const CalculatorEvaluation *evaluation,
    CalculatorError *error
)
{
    BigDecimal *operand;
    BigDecimal *value;
    BigInt *integer = NULL;
    BigInt *integer_result = NULL;
    CalculatorStatus status =
        calculator_evaluate_expression(&operand, expression->data.postfix.operand, evaluation, error);

    if (status != CALCULATOR_OK)
    {
        return status;
    }

    if (expression->data.postfix.operation == CALCULATOR_POSTFIX_SQUARE ||
        expression->data.postfix.operation == CALCULATOR_POSTFIX_CUBE)
    {
        value = bigdecimal_create();

        if (value == NULL)
        {
            bigdecimal_destroy(operand);
            calculator_error_set(error, CALCULATOR_OUT_OF_MEMORY, expression->offset);

            return CALCULATOR_OUT_OF_MEMORY;
        }

        status = calculator_from_bigdecimal_status(bigdecimal_mul(value, operand, operand));

        if (status == CALCULATOR_OK && expression->data.postfix.operation == CALCULATOR_POSTFIX_CUBE)
        {
            status = calculator_from_bigdecimal_status(bigdecimal_mul(value, value, operand));
        }

        bigdecimal_destroy(operand);

        if (status == CALCULATOR_OK && calculator_time_limit_reached(evaluation))
        {
            status = CALCULATOR_TIME_LIMIT;
        }

        if (status != CALCULATOR_OK)
        {
            bigdecimal_destroy(value);
            calculator_error_set(error, status, expression->offset);

            return status;
        }

        *result = value;

        return CALCULATOR_OK;
    }

    if (expression->data.postfix.operation != CALCULATOR_POSTFIX_FACTORIAL)
    {
        bigdecimal_destroy(operand);
        calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, expression->offset);

        return CALCULATOR_INVALID_ARGUMENT;
    }

    status = calculator_check_factorial_limit(operand);

    if (status == CALCULATOR_OK)
    {
        status = calculator_bigdecimal_to_bigint(&integer, operand, false);
    }

    bigdecimal_destroy(operand);

    if (status != CALCULATOR_OK)
    {
        calculator_error_set(error, status, expression->offset);

        return status;
    }

    integer_result = bigint_create();

    if (integer_result == NULL)
    {
        bigint_destroy(integer);
        calculator_error_set(error, CALCULATOR_OUT_OF_MEMORY, expression->offset);

        return CALCULATOR_OUT_OF_MEMORY;
    }

    status = calculator_from_bigint_status(bigint_factorial(integer_result, integer));
    bigint_destroy(integer);

    if (status == CALCULATOR_OK && calculator_time_limit_reached(evaluation))
    {
        status = CALCULATOR_TIME_LIMIT;
    }

    if (status != CALCULATOR_OK)
    {
        bigint_destroy(integer_result);
        calculator_error_set(error, status, expression->offset);

        return status;
    }

    value = bigdecimal_create();

    if (value == NULL)
    {
        bigint_destroy(integer_result);
        calculator_error_set(error, CALCULATOR_OUT_OF_MEMORY, expression->offset);

        return CALCULATOR_OUT_OF_MEMORY;
    }

    status = calculator_set_bigdecimal_from_bigint(value, integer_result);
    bigint_destroy(integer_result);

    if (status != CALCULATOR_OK)
    {
        bigdecimal_destroy(value);
        calculator_error_set(error, status, expression->offset);

        return status;
    }

    *result = value;

    return CALCULATOR_OK;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Integer-valued calls and their application domain checks.
------------------------------------------------------------------------------------------------------------------------------
*/

static CalculatorStatus calculator_evaluate_integer_call(
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

static CalculatorStatus calculator_evaluate_root_call(
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

static CalculatorStatus calculator_evaluate_transcendental_call(
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

static CalculatorStatus calculator_evaluate_trigonometric_call(
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

/* Evaluate basic calls left to right, retaining at most the selected value
 * and the current argument. Even an unselected argument must be evaluated
 * so that its errors are not silently discarded. */

/*
------------------------------------------------------------------------------------------------------------------------------
    Decimal selection calls: absolute value, sign, minimum, and maximum.
------------------------------------------------------------------------------------------------------------------------------
*/

static CalculatorStatus calculator_evaluate_basic_call(
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

static CalculatorStatus calculator_evaluate_aggregate_call(
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

static CalculatorStatus calculator_evaluate_rounding_call(
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

static CalculatorStatus calculator_evaluate_expression(
    BigDecimal **result,
    const CalculatorExpression *expression,
    const CalculatorEvaluation *evaluation,
    CalculatorError *error
)
{
    BigDecimal *value;
    CalculatorStatus status;

    if (calculator_time_limit_reached(evaluation))
    {
        calculator_error_set(error, CALCULATOR_TIME_LIMIT, expression->offset);

        return CALCULATOR_TIME_LIMIT;
    }

    if (expression->type == CALCULATOR_EXPRESSION_CALL)
    {
        /* Borrow children for the existing operator path; this temporary node
         * owns nothing and must never be passed to expression_destroy. */
        CalculatorExpression operation = {0};
        operation.offset = expression->offset;
        operation.depth = expression->depth;

        switch (expression->data.call.function->implementation)
        {
            case CALCULATOR_FUNCTION_SQRT:
            case CALCULATOR_FUNCTION_CBRT:
            case CALCULATOR_FUNCTION_ROOT:
                return calculator_evaluate_root_call(result, expression, evaluation, error);
            case CALCULATOR_FUNCTION_EXP:
            case CALCULATOR_FUNCTION_LN:
            case CALCULATOR_FUNCTION_LOG:
                return calculator_evaluate_transcendental_call(result, expression, evaluation, error);
            case CALCULATOR_FUNCTION_SIN:
            case CALCULATOR_FUNCTION_COS:
            case CALCULATOR_FUNCTION_TAN:
            case CALCULATOR_FUNCTION_ASIN:
            case CALCULATOR_FUNCTION_ACOS:
            case CALCULATOR_FUNCTION_ATAN:
            case CALCULATOR_FUNCTION_RADIANS:
            case CALCULATOR_FUNCTION_DEGREES:
                return calculator_evaluate_trigonometric_call(result, expression, evaluation, error);
            case CALCULATOR_FUNCTION_GCD:
            case CALCULATOR_FUNCTION_LCM:
            case CALCULATOR_FUNCTION_MOD:
            case CALCULATOR_FUNCTION_NPR:
            case CALCULATOR_FUNCTION_NCR:
            case CALCULATOR_FUNCTION_ISQRT:
                return calculator_evaluate_integer_call(result, expression, evaluation, error);
            case CALCULATOR_FUNCTION_ABS:
            case CALCULATOR_FUNCTION_SIGN:
            case CALCULATOR_FUNCTION_MIN:
            case CALCULATOR_FUNCTION_MAX:
                return calculator_evaluate_basic_call(result, expression, evaluation, error);
            case CALCULATOR_FUNCTION_SUM:
            case CALCULATOR_FUNCTION_PRODUCT:
            case CALCULATOR_FUNCTION_MEAN:
            case CALCULATOR_FUNCTION_VARIANCE:
            case CALCULATOR_FUNCTION_STANDARD_DEVIATION_POPULATION:
            case CALCULATOR_FUNCTION_STANDARD_DEVIATION_SAMPLE:
                return calculator_evaluate_aggregate_call(result, expression, evaluation, error);
            case CALCULATOR_FUNCTION_FLOOR:
            case CALCULATOR_FUNCTION_CEIL:
            case CALCULATOR_FUNCTION_TRUNC:
            case CALCULATOR_FUNCTION_ROUND:
                return calculator_evaluate_rounding_call(result, expression, evaluation, error);
            case CALCULATOR_FUNCTION_POWER:
                operation.type = CALCULATOR_EXPRESSION_BINARY;
                operation.data.binary.operation = CALCULATOR_BINARY_POWER;
                operation.data.binary.left = expression->data.call.arguments[0];
                operation.data.binary.right = expression->data.call.arguments[1];
                break;
            case CALCULATOR_FUNCTION_FACTORIAL:
                operation.type = CALCULATOR_EXPRESSION_POSTFIX;
                operation.data.postfix.operation = CALCULATOR_POSTFIX_FACTORIAL;
                operation.data.postfix.operand = expression->data.call.arguments[0];
                break;
            default:
                calculator_error_set(error, CALCULATOR_NOT_IMPLEMENTED, expression->offset);

                return CALCULATOR_NOT_IMPLEMENTED;
        }

        return calculator_evaluate_expression(result, &operation, evaluation, error);
    }

    if (expression->type == CALCULATOR_EXPRESSION_NUMBER)
    {
        value = bigdecimal_create();

        if (value == NULL)
        {
            calculator_error_set(error, CALCULATOR_OUT_OF_MEMORY, expression->offset);

            return CALCULATOR_OUT_OF_MEMORY;
        }

        status = calculator_set_number(value, expression->data.number.text);

        if (status != CALCULATOR_OK)
        {
            bigdecimal_destroy(value);
            calculator_error_set(error, status, expression->offset);

            return status;
        }

        *result = value;

        return CALCULATOR_OK;
    }

    if (expression->type == CALCULATOR_EXPRESSION_CONSTANT)
    {
        size_t constant_index = (size_t)expression->data.constant.constant;

        if (constant_index >= CALCULATOR_CONSTANT_COUNT)
        {
            calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, expression->offset);

            return CALCULATOR_INVALID_ARGUMENT;
        }

        if (evaluation->constant_cache->values[constant_index] == NULL ||
            evaluation->constant_cache->digits[constant_index] != evaluation->context->division_scale)
        {
            BigDecimal *constant = bigdecimal_create();

            if (constant == NULL)
            {
                calculator_error_set(error, CALCULATOR_OUT_OF_MEMORY, expression->offset);

                return CALCULATOR_OUT_OF_MEMORY;
            }

            status = calculator_constant_set_value(
                constant,
                expression->data.constant.constant,
                evaluation->context->division_scale,
                evaluation->context->rounding);

            if (status != CALCULATOR_OK)
            {
                bigdecimal_destroy(constant);
                calculator_error_set(error, status, expression->offset);

                return status;
            }

            bigdecimal_destroy(evaluation->constant_cache->values[constant_index]);
            evaluation->constant_cache->values[constant_index] = constant;
            evaluation->constant_cache->digits[constant_index] = evaluation->context->division_scale;
        }

        value = bigdecimal_create();

        if (value == NULL)
        {
            calculator_error_set(error, CALCULATOR_OUT_OF_MEMORY, expression->offset);

            return CALCULATOR_OUT_OF_MEMORY;
        }

        status = calculator_from_bigdecimal_status(
            bigdecimal_copy(value, evaluation->constant_cache->values[constant_index]));

        if (status != CALCULATOR_OK)
        {
            bigdecimal_destroy(value);
            calculator_error_set(error, status, expression->offset);

            return status;
        }

        *result = value;

        return CALCULATOR_OK;
    }

    if (expression->type == CALCULATOR_EXPRESSION_UNARY)
    {
        BigDecimal *operand;

        status = calculator_evaluate_expression(&operand, expression->data.unary.operand, evaluation, error);

        if (status != CALCULATOR_OK)
        {
            return status;
        }

        if (expression->data.unary.operation == CALCULATOR_UNARY_PLUS)
        {
            *result = operand;

            return CALCULATOR_OK;
        }

        if (expression->data.unary.operation != CALCULATOR_UNARY_MINUS)
        {
            bigdecimal_destroy(operand);
            calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, expression->offset);

            return CALCULATOR_INVALID_ARGUMENT;
        }

        value = bigdecimal_create();

        if (value == NULL)
        {
            bigdecimal_destroy(operand);
            calculator_error_set(error, CALCULATOR_OUT_OF_MEMORY, expression->offset);

            return CALCULATOR_OUT_OF_MEMORY;
        }

        status = calculator_from_bigdecimal_status(bigdecimal_negate(value, operand));
        bigdecimal_destroy(operand);

        if (status != CALCULATOR_OK)
        {
            bigdecimal_destroy(value);
            calculator_error_set(error, status, expression->offset);

            return status;
        }

        *result = value;

        return CALCULATOR_OK;
    }

    if (expression->type == CALCULATOR_EXPRESSION_POSTFIX)
    {
        return calculator_evaluate_postfix(result, expression, evaluation, error);
    }

    if (expression->type == CALCULATOR_EXPRESSION_BINARY)
    {
        BigDecimal *left;
        BigDecimal *right;

        status = calculator_evaluate_expression(&left, expression->data.binary.left, evaluation, error);

        if (status != CALCULATOR_OK)
        {
            return status;
        }

        status = calculator_evaluate_expression(&right, expression->data.binary.right, evaluation, error);

        if (status != CALCULATOR_OK)
        {
            bigdecimal_destroy(left);

            return status;
        }

        value = bigdecimal_create();

        if (value == NULL)
        {
            bigdecimal_destroy(left);
            bigdecimal_destroy(right);
            calculator_error_set(error, CALCULATOR_OUT_OF_MEMORY, expression->offset);

            return CALCULATOR_OUT_OF_MEMORY;
        }

        switch (expression->data.binary.operation)
        {
            case CALCULATOR_BINARY_ADD:
                status = calculator_from_bigdecimal_status(bigdecimal_add(value, left, right));
                break;
            case CALCULATOR_BINARY_SUBTRACT:
                status = calculator_from_bigdecimal_status(bigdecimal_sub(value, left, right));
                break;
            case CALCULATOR_BINARY_MULTIPLY:
                status = calculator_from_bigdecimal_status(bigdecimal_mul(value, left, right));
                break;
            case CALCULATOR_BINARY_DIVIDE:
                status = calculator_from_bigdecimal_status(
                    evaluation->context->significant_division
                        ? bigdecimal_div_exact_or_significant(value,
                                                              left,
                                                              right,
                                                              evaluation->context->division_scale,
                                                              evaluation->context->rounding)
                        : bigdecimal_div(value,
                                         left,
                                         right,
                                         evaluation->context->division_scale,
                                         evaluation->context->rounding));
                break;
            case CALCULATOR_BINARY_POWER:
                status = calculator_bigdecimal_pow(value, left, right, evaluation);
                break;
            default:
                status = CALCULATOR_INVALID_ARGUMENT;
                break;
        }

        if (status == CALCULATOR_OK && calculator_time_limit_reached(evaluation))
        {
            status = CALCULATOR_TIME_LIMIT;
        }

        bigdecimal_destroy(left);
        bigdecimal_destroy(right);

        if (status != CALCULATOR_OK)
        {
            bigdecimal_destroy(value);
            calculator_error_set(error, status, expression->offset);

            return status;
        }

        *result = value;

        return CALCULATOR_OK;
    }

    calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, expression->offset);

    return CALCULATOR_INVALID_ARGUMENT;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Evaluator operation functions.
------------------------------------------------------------------------------------------------------------------------------
*/

static CalculatorStatus calculator_evaluate_impl(
    BigDecimal *result,
    const CalculatorExpression *expression,
    const CalculatorContext *context,
    CalculatorError *error
)
{
    CalculatorEvaluation evaluation;
    CalculatorConstantCache constant_cache;
    BigDecimal *temporary;
    CalculatorStatus status;

    if (result == NULL || expression == NULL || context == NULL)
    {
        calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0);

        return CALCULATOR_NULL_ARGUMENT;
    }

    if (!calculator_valid_rounding(context->rounding))
    {
        calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, 0);

        return CALCULATOR_INVALID_ARGUMENT;
    }

    if (context->output_scale < CALCULATOR_UNLIMITED_OUTPUT_SCALE)
    {
        calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, 0);

        return CALCULATOR_INVALID_ARGUMENT;
    }

    if (context->time_limit_ms < 0)
    {
        calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, 0);

        return CALCULATOR_INVALID_ARGUMENT;
    }

    if (context->output_scale > CALCULATOR_MAX_OUTPUT_SCALE ||
        context->division_scale > CALCULATOR_MAX_OUTPUT_SCALE + CALCULATOR_DIVISION_GUARD_DIGITS)
    {
        calculator_error_set(error, CALCULATOR_VALUE_TOO_LARGE, 0U);

        return CALCULATOR_VALUE_TOO_LARGE;
    }

    evaluation.context = context;
    evaluation.constant_cache = &constant_cache;

    for (size_t index = 0U; index < CALCULATOR_CONSTANT_COUNT; index++)
    {
        constant_cache.values[index] = NULL;
        constant_cache.digits[index] = 0;
    }

    status = calculator_evaluate_expression(&temporary, expression, &evaluation, error);

    for (size_t index = 0U; index < CALCULATOR_CONSTANT_COUNT; index++)
    {
        bigdecimal_destroy(constant_cache.values[index]);
    }

    if (status != CALCULATOR_OK)
    {
        return status;
    }

    status = calculator_budget_status(CALCULATOR_OK);

    if (status == CALCULATOR_OK)
    {
        status = calculator_from_bigdecimal_status(bigdecimal_copy(result, temporary));
    }

    bigdecimal_destroy(temporary);

    if (status != CALCULATOR_OK)
    {
        calculator_error_set(error, status, expression->offset);

        return status;
    }

    calculator_error_clear(error);

    return CALCULATOR_OK;
}

CalculatorStatus calculator_evaluate(
    BigDecimal *result,
    const CalculatorExpression *expression,
    const CalculatorContext *context,
    CalculatorError *error
)
{
    bool owner;
    CalculatorStatus status;

    if (context == NULL)
    {
        return calculator_evaluate_impl(result, expression, context, error);
    }

    owner = numforge_budget_begin(context->time_limit_ms < 0 ? 0U : (uint64_t)context->time_limit_ms,
                                  CALCULATOR_ALLOCATION_BUDGET,
                                  CALCULATOR_SINGLE_ALLOCATION);
    status = calculator_evaluate_impl(result, expression, context, error);

    if (status != CALCULATOR_OK && context->time_limit_ms >= 0)
    {
        status = calculator_budget_status(status);
    }

    if (status != CALCULATOR_OK && (error == NULL || error->status != status))
    {
        calculator_error_set(error, status, error == NULL ? 0U : error->offset);
    }

    if (owner)
    {
        numforge_budget_end();
    }

    return status;
}
