#include "exact_evaluator.h"
#include "exact_functions.h"
#include "expression_internal.h"
#include "unit_status.h"

#include <numforge/runtime.h>

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Unsupported nodes use the decimal evaluator, which converts supported
 * exact child subtrees before approximate operations. Side-effecting calls
 * such as rand are evaluated only in the decimal path. */
bool calculator_exact_supported(const CalculatorExpression *expression, const CalculatorValue *answer)
{
    const char *marker;
    if (expression == NULL)
    {
        return false;
    }
    switch (expression->type)
    {
        case CALCULATOR_EXPRESSION_NUMBER:
            marker = strchr(expression->data.number.text, 'E');
            if (marker != NULL)
            {
                char *end;
                unsigned long long magnitude;
                errno = 0;
                magnitude = strtoull(marker + 1U + (marker[1] == '+' || marker[1] == '-'), &end, 10);
                if (errno != 0 || *end != '\0' || magnitude > 4096U)
                {
                    return false;
                }
            }
            return true;
        case CALCULATOR_EXPRESSION_VARIABLE:
            return expression->data.variable.value != NULL && expression->data.variable.value->kind != CALCULATOR_VALUE_DECIMAL;
        case CALCULATOR_EXPRESSION_ANSWER:
            return answer != NULL && answer->kind != CALCULATOR_VALUE_DECIMAL;
        case CALCULATOR_EXPRESSION_UNARY:
            return calculator_exact_supported(expression->data.unary.operand, answer);
        case CALCULATOR_EXPRESSION_BINARY:
            return calculator_exact_supported(expression->data.binary.left, answer) &&
                   calculator_exact_supported(expression->data.binary.right, answer);
        case CALCULATOR_EXPRESSION_POSTFIX:
            return (expression->data.postfix.operation == CALCULATOR_POSTFIX_SQUARE ||
                    expression->data.postfix.operation == CALCULATOR_POSTFIX_CUBE ||
                    expression->data.postfix.operation == CALCULATOR_POSTFIX_FACTORIAL) &&
                   calculator_exact_supported(expression->data.postfix.operand, answer);
        case CALCULATOR_EXPRESSION_CALL:
            switch (expression->data.call.function->implementation)
            {
                case CALCULATOR_FUNCTION_CONVERT:
                    if (!numforge_unit_conversion_is_exact(expression->data.call.from_unit,
                                                           expression->data.call.to_unit)) return false;
                    break;
                case CALCULATOR_FUNCTION_SQRT:
                case CALCULATOR_FUNCTION_CBRT:
                case CALCULATOR_FUNCTION_ROOT:
                case CALCULATOR_FUNCTION_POWER:
                case CALCULATOR_FUNCTION_ABS:
                case CALCULATOR_FUNCTION_SIGN:
                case CALCULATOR_FUNCTION_MIN:
                case CALCULATOR_FUNCTION_MAX:
                case CALCULATOR_FUNCTION_SUM:
                case CALCULATOR_FUNCTION_PRODUCT:
                case CALCULATOR_FUNCTION_MEAN:
                case CALCULATOR_FUNCTION_MEDIAN:
                case CALCULATOR_FUNCTION_GEOMETRIC_MEAN:
                case CALCULATOR_FUNCTION_HARMONIC_MEAN:
                case CALCULATOR_FUNCTION_VARIANCE:
                case CALCULATOR_FUNCTION_STANDARD_DEVIATION_POPULATION:
                case CALCULATOR_FUNCTION_STANDARD_DEVIATION_SAMPLE:
                case CALCULATOR_FUNCTION_FACTORIAL:
                case CALCULATOR_FUNCTION_ISQRT:
                case CALCULATOR_FUNCTION_GCD:
                case CALCULATOR_FUNCTION_LCM:
                case CALCULATOR_FUNCTION_MOD:
                case CALCULATOR_FUNCTION_NPR:
                case CALCULATOR_FUNCTION_NCR:
                case CALCULATOR_FUNCTION_FLOOR:
                case CALCULATOR_FUNCTION_CEIL:
                case CALCULATOR_FUNCTION_TRUNC:
                case CALCULATOR_FUNCTION_ROUND:
                    break;
                default:
                    return false;
            }
            for (size_t index = 0U; index < expression->data.call.count; index++)
            {
                if (!calculator_exact_supported(expression->data.call.arguments[index], answer))
                {
                    return false;
                }
            }
            return true;
        default:
            return false;
    }
}

static CalculatorStatus calculator_exact_recursive(BigRational **result, const CalculatorExpression *expression,
                                                   const CalculatorValue *answer, CalculatorError *error);

static CalculatorStatus calculator_exact_count(BigRational *result, size_t count)
{
    BigInt *integer = bigint_create();
    char digits[32];
    CalculatorStatus status = CALCULATOR_OUT_OF_MEMORY;

    if (integer != NULL)
    {
        (void)snprintf(digits, sizeof(digits), "%zu", count);
        status = calculator_from_integer_status(bigint_set_string(integer, digits));
        if (status == CALCULATOR_OK)
        {
            status = calculator_from_rational_status(bigrational_from_bigint(result, integer));
        }
    }
    bigint_destroy(integer);
    return status;
}

static CalculatorStatus calculator_exact_statistics(BigRational *result, BigRational *const *items, size_t count,
                                                    CalculatorFunctionImplementation function)
{
    BigRational *accumulator = bigrational_create();
    BigRational *work = bigrational_create();
    BigRational *divisor = bigrational_create();
    BigRational *zero = bigrational_create();
    CalculatorStatus status = CALCULATOR_OUT_OF_MEMORY;
    int comparison;

    if (accumulator == NULL || work == NULL || divisor == NULL || zero == NULL)
    {
        goto done;
    }
    if (function == CALCULATOR_FUNCTION_GEOMETRIC_MEAN || function == CALCULATOR_FUNCTION_HARMONIC_MEAN)
    {
        status = calculator_exact_count(divisor, 1U);
        for (size_t index = 0U; index < count && status == CALCULATOR_OK; index++)
        {
            status = calculator_from_rational_status(bigrational_compare(&comparison, items[index], zero));
            if (status != CALCULATOR_OK)
            {
                break;
            }
            if (comparison < 0 || (comparison == 0 && function == CALCULATOR_FUNCTION_HARMONIC_MEAN))
            {
                status = CALCULATOR_INVALID_ARGUMENT;
                break;
            }
            if (function == CALCULATOR_FUNCTION_GEOMETRIC_MEAN)
            {
                status = calculator_from_rational_status(bigrational_mul(divisor, divisor, items[index]));
            }
            else
            {
                status = calculator_from_rational_status(bigrational_div(work, divisor, items[index]));
                if (status == CALCULATOR_OK)
                {
                    status = calculator_from_rational_status(bigrational_add(accumulator, accumulator, work));
                }
            }
        }
        if (status == CALCULATOR_OK && function == CALCULATOR_FUNCTION_GEOMETRIC_MEAN)
        {
#if SIZE_MAX > UINT32_MAX
            if (count > UINT32_MAX)
            {
                status = CALCULATOR_VALUE_TOO_LARGE;
            }
            else
#endif
            {
                status = calculator_exact_nth_root(result, divisor, (uint32_t)count);
            }
        }
        else if (status == CALCULATOR_OK)
        {
            status = calculator_exact_count(divisor, count);
            if (status == CALCULATOR_OK)
            {
                status = calculator_from_rational_status(bigrational_div(result, divisor, accumulator));
            }
        }
        goto done;
    }

    status = calculator_exact_count(divisor, count);
    for (size_t index = 0U; index < count && status == CALCULATOR_OK; index++)
    {
        status = calculator_from_rational_status(bigrational_add(accumulator, accumulator, items[index]));
    }
    if (status == CALCULATOR_OK)
    {
        status = calculator_from_rational_status(bigrational_div(accumulator, accumulator, divisor));
    }
    for (size_t index = 0U; index < count && status == CALCULATOR_OK; index++)
    {
        status = calculator_from_rational_status(bigrational_sub(work, items[index], accumulator));
        if (status == CALCULATOR_OK)
        {
            status = calculator_from_rational_status(bigrational_mul(work, work, work));
        }
        if (status == CALCULATOR_OK)
        {
            status = calculator_from_rational_status(bigrational_add(result, result, work));
        }
    }
    if (status == CALCULATOR_OK)
    {
        status = calculator_exact_count(divisor,
                                        function == CALCULATOR_FUNCTION_STANDARD_DEVIATION_SAMPLE ? count - 1U : count);
    }
    if (status == CALCULATOR_OK)
    {
        status = calculator_from_rational_status(bigrational_div(result, result, divisor));
    }
    if (status == CALCULATOR_OK && function != CALCULATOR_FUNCTION_VARIANCE)
    {
        status = calculator_exact_sqrt(result, result);
    }

done:
    bigrational_destroy(accumulator);
    bigrational_destroy(work);
    bigrational_destroy(divisor);
    bigrational_destroy(zero);
    return status;
}

static CalculatorStatus calculator_exact_call(BigRational *result, const CalculatorExpression *expression,
                                              const CalculatorValue *answer, CalculatorError *error)
{
    size_t count = expression->data.call.count;
    CalculatorFunctionImplementation function = expression->data.call.function->implementation;
    BigRational **items = numforge_calloc(count, sizeof(*items));
    BigRational *divisor = NULL;
    BigInt *integer = NULL;
    CalculatorStatus status = CALCULATOR_OUT_OF_MEMORY;
    int comparison;

    if (items == NULL)
    {
        goto done;
    }
    for (size_t index = 0U; index < count; index++)
    {
        status = calculator_exact_recursive(&items[index], expression->data.call.arguments[index], answer, error);
        if (status != CALCULATOR_OK)
        {
            goto done;
        }
    }
    status = CALCULATOR_OK;
    switch (function)
    {
        case CALCULATOR_FUNCTION_SQRT:
            status = calculator_exact_sqrt(result, items[0]);
            break;
        case CALCULATOR_FUNCTION_CBRT:
            status = calculator_exact_nth_root(result, items[0], 3U);
            break;
        case CALCULATOR_FUNCTION_ROOT:
        {
            BigInt *top = bigint_create();
            BigInt *bottom = bigint_create();
            char *digits = NULL;
            unsigned long degree = 0U;
            status = top == NULL || bottom == NULL ? CALCULATOR_OUT_OF_MEMORY : CALCULATOR_OK;
            if (status == CALCULATOR_OK)
            {
                status = calculator_from_rational_status(bigrational_get_numerator(top, items[1]));
            }
            if (status == CALCULATOR_OK)
            {
                status = calculator_from_rational_status(bigrational_get_denominator(bottom, items[1]));
            }
            if (status == CALCULATOR_OK && (!bigint_is_one(bottom) || bigint_is_negative(top) || bigint_is_zero(top)))
            {
                status = CALCULATOR_INVALID_ARGUMENT;
            }
            if (status == CALCULATOR_OK)
            {
                digits = bigint_to_string(top);
                status = digits == NULL ? CALCULATOR_OUT_OF_MEMORY : CALCULATOR_OK;
            }
            if (status == CALCULATOR_OK)
            {
                errno = 0;
                degree = strtoul(digits, NULL, 10);
                if (errno != 0 || degree > CALCULATOR_MAX_ROOT_DEGREE)
                {
                    status = CALCULATOR_VALUE_TOO_LARGE;
                }
            }
            if (status == CALCULATOR_OK)
            {
                status = calculator_exact_nth_root(result, items[0], (uint32_t)degree);
            }
            free(digits);
            bigint_destroy(top);
            bigint_destroy(bottom);
        }
        break;
        case CALCULATOR_FUNCTION_POWER:
            status = calculator_exact_power(result, items[0], items[1]);
            break;
        case CALCULATOR_FUNCTION_FACTORIAL:
        case CALCULATOR_FUNCTION_ISQRT:
        case CALCULATOR_FUNCTION_GCD:
        case CALCULATOR_FUNCTION_LCM:
        case CALCULATOR_FUNCTION_MOD:
        case CALCULATOR_FUNCTION_NPR:
        case CALCULATOR_FUNCTION_NCR:
            status = calculator_exact_integer_operation(result, items, count, function);
            break;
        case CALCULATOR_FUNCTION_FLOOR:
        case CALCULATOR_FUNCTION_CEIL:
        case CALCULATOR_FUNCTION_TRUNC:
        case CALCULATOR_FUNCTION_ROUND:
            status = calculator_exact_rounding(result, items, count, function);
            break;
        case CALCULATOR_FUNCTION_ABS:
            status = calculator_from_rational_status(bigrational_abs(result, items[0]));
            break;
        case CALCULATOR_FUNCTION_SIGN:
            integer = bigint_create();
            divisor = bigrational_create();
            if (integer == NULL || divisor == NULL)
            {
                status = CALCULATOR_OUT_OF_MEMORY;
                break;
            }
            status = calculator_from_rational_status(bigrational_compare(&comparison, items[0], divisor));
            if (status == CALCULATOR_OK)
            {
                status = calculator_from_integer_status(bigint_set_string(integer, comparison < 0   ? "-1"
                                                                                   : comparison > 0 ? "1"
                                                                                                    : "0"));
            }
            if (status == CALCULATOR_OK)
            {
                status = calculator_from_rational_status(bigrational_from_bigint(result, integer));
            }
            break;
        case CALCULATOR_FUNCTION_MIN:
        case CALCULATOR_FUNCTION_MAX:
        {
            size_t chosen = 0U;
            for (size_t index = 1U; index < count && status == CALCULATOR_OK; index++)
            {
                status = calculator_from_rational_status(bigrational_compare(&comparison, items[index], items[chosen]));
                if (status == CALCULATOR_OK && ((function == CALCULATOR_FUNCTION_MIN && comparison < 0) ||
                                                (function == CALCULATOR_FUNCTION_MAX && comparison > 0)))
                {
                    chosen = index;
                }
            }
            if (status == CALCULATOR_OK)
            {
                status = calculator_from_rational_status(bigrational_copy(result, items[chosen]));
            }
        }
        break;
        case CALCULATOR_FUNCTION_SUM:
        case CALCULATOR_FUNCTION_PRODUCT:
        case CALCULATOR_FUNCTION_MEAN:
            status = calculator_from_rational_status(bigrational_copy(result, items[0]));
            for (size_t index = 1U; index < count && status == CALCULATOR_OK; index++)
            {
                status = calculator_from_rational_status(function == CALCULATOR_FUNCTION_PRODUCT
                                                             ? bigrational_mul(result, result, items[index])
                                                             : bigrational_add(result, result, items[index]));
            }
            if (status == CALCULATOR_OK && function == CALCULATOR_FUNCTION_MEAN)
            {
                char digits[32];
                integer = bigint_create();
                divisor = bigrational_create();
                status = integer == NULL || divisor == NULL ? CALCULATOR_OUT_OF_MEMORY : CALCULATOR_OK;
                if (status == CALCULATOR_OK)
                {
                    (void)snprintf(digits, sizeof(digits), "%zu", count);
                    status = calculator_from_integer_status(bigint_set_string(integer, digits));
                }
                if (status == CALCULATOR_OK)
                {
                    status = calculator_from_rational_status(bigrational_from_bigint(divisor, integer));
                }
                if (status == CALCULATOR_OK)
                {
                    status = calculator_from_rational_status(bigrational_div(result, result, divisor));
                }
            }
            break;
        case CALCULATOR_FUNCTION_MEDIAN:
            for (size_t index = 1U; index < count && status == CALCULATOR_OK; index++)
            {
                size_t position = index;
                while (position > 0U)
                {
                    status = calculator_from_rational_status(
                        bigrational_compare(&comparison, items[position - 1U], items[position]));
                    if (status != CALCULATOR_OK || comparison <= 0)
                    {
                        break;
                    }
                    BigRational *swap = items[position - 1U];
                    items[position - 1U] = items[position];
                    items[position] = swap;
                    position--;
                }
            }
            if (status == CALCULATOR_OK)
            {
                status = calculator_from_rational_status(bigrational_copy(result, items[count / 2U]));
            }
            if (status == CALCULATOR_OK && count % 2U == 0U)
            {
                integer = bigint_create();
                divisor = bigrational_create();
                status = integer == NULL || divisor == NULL ? CALCULATOR_OUT_OF_MEMORY : CALCULATOR_OK;
                if (status == CALCULATOR_OK)
                {
                    status = calculator_from_rational_status(bigrational_add(result, result, items[count / 2U - 1U]));
                }
                if (status == CALCULATOR_OK)
                {
                    status = calculator_from_integer_status(bigint_set_string(integer, "2"));
                }
                if (status == CALCULATOR_OK)
                {
                    status = calculator_from_rational_status(bigrational_from_bigint(divisor, integer));
                }
                if (status == CALCULATOR_OK)
                {
                    status = calculator_from_rational_status(bigrational_div(result, result, divisor));
                }
            }
            break;
        case CALCULATOR_FUNCTION_GEOMETRIC_MEAN:
        case CALCULATOR_FUNCTION_HARMONIC_MEAN:
        case CALCULATOR_FUNCTION_VARIANCE:
        case CALCULATOR_FUNCTION_STANDARD_DEVIATION_POPULATION:
        case CALCULATOR_FUNCTION_STANDARD_DEVIATION_SAMPLE:
            status = calculator_exact_statistics(result, items, count, function);
            break;
        default:
            status = CALCULATOR_NOT_IMPLEMENTED;
            break;
    }
done:
    if (items != NULL)
    {
        for (size_t index = 0U; index < count; index++)
        {
            bigrational_destroy(items[index]);
        }
    }
    free(items);
    bigrational_destroy(divisor);
    bigint_destroy(integer);
    return status;
}

static CalculatorStatus calculator_exact_recursive(BigRational **result, const CalculatorExpression *expression,
                                                   const CalculatorValue *answer, CalculatorError *error)
{
    BigRational *value = NULL;
    BigRational *left = NULL;
    BigRational *right = NULL;
    BigDecimal *decimal = NULL;
    CalculatorStatus status = CALCULATOR_OUT_OF_MEMORY;
    BigRationalStatus rational_status = BIGRATIONAL_OK;

    if (!numforge_budget_check())
    {
        status = calculator_budget_status(CALCULATOR_OUT_OF_MEMORY);
        goto done;
    }
    value = bigrational_create();
    if (value == NULL)
    {
        goto done;
    }
    switch (expression->type)
    {
        case CALCULATOR_EXPRESSION_NUMBER:
            decimal = bigdecimal_create();
            if (decimal == NULL)
            {
                goto done;
            }
            {
                const char *text = expression->data.number.text;
                const char *separator = strchr(text, ',');
                char *normalized = NULL;
                BigDecimalStatus decimal_status;
                if (separator != NULL)
                {
                    size_t length = strlen(text);
                    normalized = numforge_malloc(length + 1U);
                    if (normalized == NULL)
                    {
                        goto done;
                    }
                    memcpy(normalized, text, length + 1U);
                    normalized[separator - text] = '.';
                    text = normalized;
                }
                decimal_status = bigdecimal_set_string(decimal, text);
                free(normalized);
                if (decimal_status != BIGDECIMAL_OK)
                {
                    status = decimal_status == BIGDECIMAL_OUT_OF_MEMORY     ? CALCULATOR_OUT_OF_MEMORY
                             : decimal_status == BIGDECIMAL_VALUE_TOO_LARGE ? CALCULATOR_VALUE_TOO_LARGE
                             : decimal_status == BIGDECIMAL_SCALE_OVERFLOW  ? CALCULATOR_SCALE_OVERFLOW
                                                                            : CALCULATOR_INVALID_ARGUMENT;
                    goto done;
                }
            }
            rational_status = bigrational_from_bigdecimal(value, decimal);
            break;
        case CALCULATOR_EXPRESSION_VARIABLE:
            rational_status = expression->data.variable.value->integer != NULL
                ? bigrational_from_bigint(value, expression->data.variable.value->integer)
                : bigrational_copy(value, expression->data.variable.value->rational);
            break;
        case CALCULATOR_EXPRESSION_ANSWER:
            rational_status = answer->integer != NULL ? bigrational_from_bigint(value, answer->integer)
                                                      : bigrational_copy(value, answer->rational);
            break;
        case CALCULATOR_EXPRESSION_UNARY:
            status = calculator_exact_recursive(&left, expression->data.unary.operand, answer, error);
            if (status != CALCULATOR_OK)
            {
                goto done;
            }
            rational_status = expression->data.unary.operation == CALCULATOR_UNARY_MINUS
                                  ? bigrational_negate(value, left)
                                  : bigrational_copy(value, left);
            break;
        case CALCULATOR_EXPRESSION_POSTFIX:
            status = calculator_exact_recursive(&left, expression->data.postfix.operand, answer, error);
            if (status != CALCULATOR_OK)
            {
                goto done;
            }
            if (expression->data.postfix.operation == CALCULATOR_POSTFIX_FACTORIAL)
            {
                status = calculator_exact_integer_operation(value, &left, 1U, CALCULATOR_FUNCTION_FACTORIAL);
                if (status != CALCULATOR_OK)
                {
                    goto done;
                }
                break;
            }
            {
                BigInt *exponent = bigint_create();
                if (exponent == NULL)
                {
                    status = CALCULATOR_OUT_OF_MEMORY;
                    goto done;
                }
                status = calculator_from_integer_status(bigint_set_string(
                    exponent, expression->data.postfix.operation == CALCULATOR_POSTFIX_SQUARE ? "2" : "3"));
                if (status == CALCULATOR_OK)
                {
                    right = bigrational_create();
                    status = right == NULL ? CALCULATOR_OUT_OF_MEMORY
                                           : calculator_from_rational_status(bigrational_from_bigint(right, exponent));
                }
                bigint_destroy(exponent);
                if (status != CALCULATOR_OK)
                {
                    goto done;
                }
            }
            status = calculator_exact_power(value, left, right);
            if (status != CALCULATOR_OK)
            {
                goto done;
            }
            break;
        case CALCULATOR_EXPRESSION_BINARY:
            status = calculator_exact_recursive(&left, expression->data.binary.left, answer, error);
            if (status != CALCULATOR_OK)
            {
                goto done;
            }
            status = calculator_exact_recursive(&right, expression->data.binary.right, answer, error);
            if (status != CALCULATOR_OK)
            {
                goto done;
            }
            switch (expression->data.binary.operation)
            {
                case CALCULATOR_BINARY_ADD:
                    rational_status = bigrational_add(value, left, right);
                    break;
                case CALCULATOR_BINARY_SUBTRACT:
                    rational_status = bigrational_sub(value, left, right);
                    break;
                case CALCULATOR_BINARY_MULTIPLY:
                    rational_status = bigrational_mul(value, left, right);
                    break;
                case CALCULATOR_BINARY_DIVIDE:
                    rational_status = bigrational_div(value, left, right);
                    break;
                case CALCULATOR_BINARY_POWER:
                    status = calculator_exact_power(value, left, right);
                    if (status != CALCULATOR_OK)
                    {
                        goto done;
                    }
                    break;
                default:
                    status = CALCULATOR_INVALID_ARGUMENT;
                    goto done;
            }
            break;
        case CALCULATOR_EXPRESSION_CALL:
            if (expression->data.call.function->implementation == CALCULATOR_FUNCTION_CONVERT)
            {
                status = calculator_exact_recursive(&left, expression->data.call.arguments[0], answer, error);
                if (status == CALCULATOR_OK)
                    status = calculator_from_unit_status(numforge_unit_convert_rational(value, left,
                        expression->data.call.from_unit, expression->data.call.to_unit));
            }
            else status = calculator_exact_call(value, expression, answer, error);
            if (status != CALCULATOR_OK)
            {
                goto done;
            }
            break;
        default:
            status = CALCULATOR_INVALID_ARGUMENT;
            goto done;
    }
    status = calculator_from_rational_status(rational_status);
done:
    bigdecimal_destroy(decimal);
    bigrational_destroy(left);
    bigrational_destroy(right);
    if (status == CALCULATOR_OK)
    {
        *result = value;
    }
    else
    {
        bigrational_destroy(value);
        if (error == NULL || error->status != status)
        {
            calculator_error_set(error, status, expression->offset);
        }
    }
    return status;
}

CalculatorStatus calculator_evaluate_exact(BigRational **result, const CalculatorExpression *expression,
                                           const CalculatorValue *answer, CalculatorError *error)
{
    if (result == NULL || expression == NULL)
    {
        calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0U);
        return CALCULATOR_NULL_ARGUMENT;
    }
    return calculator_exact_recursive(result, expression, answer, error);
}
