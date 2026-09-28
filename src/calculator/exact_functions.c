#include "exact_functions.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

/* Arithmetic operations supported by the exact expression evaluator. */

CalculatorStatus calculator_from_rational_status(BigRationalStatus status)
{
    switch (status)
    {
        case BIGRATIONAL_OK:
            return CALCULATOR_OK;
        case BIGRATIONAL_NULL_ARGUMENT:
            return CALCULATOR_NULL_ARGUMENT;
        case BIGRATIONAL_OUT_OF_MEMORY:
            return CALCULATOR_OUT_OF_MEMORY;
        case BIGRATIONAL_DIVISION_BY_ZERO:
            return CALCULATOR_DIVISION_BY_ZERO;
        case BIGRATIONAL_VALUE_TOO_LARGE:
            return CALCULATOR_VALUE_TOO_LARGE;
        case BIGRATIONAL_SCALE_OVERFLOW:
            return CALCULATOR_SCALE_OVERFLOW;
        default:
            return CALCULATOR_INVALID_ARGUMENT;
    }
}

CalculatorStatus calculator_from_integer_status(BigIntStatus status)
{
    switch (status)
    {
        case BIGINT_OK:
            return CALCULATOR_OK;
        case BIGINT_NULL_ARGUMENT:
            return CALCULATOR_NULL_ARGUMENT;
        case BIGINT_OUT_OF_MEMORY:
            return CALCULATOR_OUT_OF_MEMORY;
        case BIGINT_DIVISION_BY_ZERO:
            return CALCULATOR_DIVISION_BY_ZERO;
        case BIGINT_VALUE_TOO_LARGE:
            return CALCULATOR_VALUE_TOO_LARGE;
        default:
            return CALCULATOR_INVALID_ARGUMENT;
    }
}

CalculatorStatus calculator_from_decimal_status(BigDecimalStatus status)
{
    switch (status)
    {
        case BIGDECIMAL_OK:
            return CALCULATOR_OK;
        case BIGDECIMAL_NULL_ARGUMENT:
            return CALCULATOR_NULL_ARGUMENT;
        case BIGDECIMAL_OUT_OF_MEMORY:
            return CALCULATOR_OUT_OF_MEMORY;
        case BIGDECIMAL_VALUE_TOO_LARGE:
            return CALCULATOR_VALUE_TOO_LARGE;
        case BIGDECIMAL_SCALE_OVERFLOW:
            return CALCULATOR_SCALE_OVERFLOW;
        default:
            return CALCULATOR_INVALID_ARGUMENT;
    }
}

CalculatorStatus calculator_exact_power(BigRational *result, const BigRational *base,
                                               const BigRational *exponent)
{
    BigInt *numerator = bigint_create();
    BigInt *denominator = bigint_create();
    BigInt *power = bigint_create();
    BigInt *top = bigint_create();
    BigInt *bottom = bigint_create();
    CalculatorStatus status = CALCULATOR_OUT_OF_MEMORY;
    bool negative;

    if (numerator == NULL || denominator == NULL || power == NULL || top == NULL || bottom == NULL)
    {
        goto done;
    }
    status = calculator_from_rational_status(bigrational_get_denominator(denominator, exponent));
    if (status != CALCULATOR_OK)
    {
        goto done;
    }
    if (!bigint_is_one(denominator))
    {
        status = CALCULATOR_INVALID_ARGUMENT;
        goto done;
    }
    status = calculator_from_rational_status(bigrational_get_numerator(power, exponent));
    if (status != CALCULATOR_OK)
    {
        goto done;
    }
    negative = bigint_is_negative(power);
    if (negative)
    {
        status = calculator_from_integer_status(bigint_abs(power, power));
        if (status != CALCULATOR_OK)
        {
            goto done;
        }
    }
    status = calculator_from_rational_status(bigrational_get_numerator(numerator, base));
    if (status == CALCULATOR_OK)
    {
        status = calculator_from_rational_status(bigrational_get_denominator(denominator, base));
    }
    if (status == CALCULATOR_OK && negative && bigint_is_zero(numerator))
    {
        status = CALCULATOR_DIVISION_BY_ZERO;
    }
    if (status == CALCULATOR_OK)
    {
        status = calculator_from_integer_status(bigint_pow(top, numerator, power));
    }
    if (status == CALCULATOR_OK)
    {
        status = calculator_from_integer_status(bigint_pow(bottom, denominator, power));
    }
    if (status == CALCULATOR_OK)
    {
        status = calculator_from_rational_status(negative ? bigrational_set_fraction(result, bottom, top)
                                                          : bigrational_set_fraction(result, top, bottom));
    }
done:
    bigint_destroy(numerator);
    bigint_destroy(denominator);
    bigint_destroy(power);
    bigint_destroy(top);
    bigint_destroy(bottom);
    return status;
}

CalculatorStatus calculator_exact_sqrt(BigRational *result, const BigRational *operand)
{
    BigInt *numerator = bigint_create();
    BigInt *denominator = bigint_create();
    BigInt *top = bigint_create();
    BigInt *bottom = bigint_create();
    BigInt *square = bigint_create();
    CalculatorStatus status = CALCULATOR_OUT_OF_MEMORY;

    if (numerator == NULL || denominator == NULL || top == NULL || bottom == NULL || square == NULL)
    {
        goto done;
    }
    status = calculator_from_rational_status(bigrational_get_numerator(numerator, operand));
    if (status == CALCULATOR_OK)
    {
        status = calculator_from_rational_status(bigrational_get_denominator(denominator, operand));
    }
    if (status == CALCULATOR_OK && bigint_is_negative(numerator))
    {
        status = CALCULATOR_INVALID_ARGUMENT;
    }
    if (status == CALCULATOR_OK)
    {
        status = calculator_from_integer_status(bigint_isqrt(top, numerator));
    }
    if (status == CALCULATOR_OK)
    {
        status = calculator_from_integer_status(bigint_mul(square, top, top));
    }
    if (status == CALCULATOR_OK && bigint_compare(square, numerator) != 0)
    {
        status = CALCULATOR_NOT_IMPLEMENTED;
    }
    if (status == CALCULATOR_OK)
    {
        status = calculator_from_integer_status(bigint_isqrt(bottom, denominator));
    }
    if (status == CALCULATOR_OK)
    {
        status = calculator_from_integer_status(bigint_mul(square, bottom, bottom));
    }
    if (status == CALCULATOR_OK && bigint_compare(square, denominator) != 0)
    {
        status = CALCULATOR_NOT_IMPLEMENTED;
    }
    if (status == CALCULATOR_OK)
    {
        status = calculator_from_rational_status(bigrational_set_fraction(result, top, bottom));
    }
done:
    bigint_destroy(numerator);
    bigint_destroy(denominator);
    bigint_destroy(top);
    bigint_destroy(bottom);
    bigint_destroy(square);
    return status;
}

static CalculatorStatus calculator_exact_integer_root(BigInt *result, const BigInt *operand, uint32_t degree)
{
    BigDecimal *input = bigdecimal_create();
    BigDecimal *output = bigdecimal_create();
    BigInt *exponent = bigint_create();
    BigInt *power = bigint_create();
    BigDecimalStatus decimal_status;
    CalculatorStatus status = CALCULATOR_OUT_OF_MEMORY;
    char digits[32];

    if (input == NULL || output == NULL || exponent == NULL || power == NULL)
    {
        goto done;
    }
    status = calculator_from_decimal_status(bigdecimal_from_bigint(input, operand));
    if (status == CALCULATOR_OK)
    {
        status = calculator_from_decimal_status(
            bigdecimal_root(output, input, degree, CALCULATOR_DEFAULT_DIVISION_SCALE, BIGDECIMAL_ROUND_HALF_EVEN));
    }
    if (status == CALCULATOR_OK)
    {
        decimal_status = bigdecimal_to_bigint(result, output);
        status = decimal_status == BIGDECIMAL_INVALID_ARGUMENT ? CALCULATOR_NOT_IMPLEMENTED
                                                               : calculator_from_decimal_status(decimal_status);
    }
    if (status == CALCULATOR_OK)
    {
        (void)snprintf(digits, sizeof(digits), "%u", degree);
        status = calculator_from_integer_status(bigint_set_string(exponent, digits));
    }
    if (status == CALCULATOR_OK)
    {
        status = calculator_from_integer_status(bigint_pow(power, result, exponent));
    }
    if (status == CALCULATOR_OK && bigint_compare(power, operand) != 0)
    {
        status = CALCULATOR_NOT_IMPLEMENTED;
    }
done:
    bigdecimal_destroy(input);
    bigdecimal_destroy(output);
    bigint_destroy(exponent);
    bigint_destroy(power);
    return status;
}

CalculatorStatus calculator_exact_nth_root(BigRational *result, const BigRational *operand, uint32_t degree)
{
    BigInt *numerator;
    BigInt *denominator;
    BigInt *top;
    BigInt *bottom;
    CalculatorStatus status = CALCULATOR_OUT_OF_MEMORY;
    bool negative;

    if (degree == 2U)
    {
        return calculator_exact_sqrt(result, operand);
    }
    numerator = bigint_create();
    denominator = bigint_create();
    top = bigint_create();
    bottom = bigint_create();
    if (numerator == NULL || denominator == NULL || top == NULL || bottom == NULL)
    {
        goto done;
    }
    status = calculator_from_rational_status(bigrational_get_numerator(numerator, operand));
    if (status == CALCULATOR_OK)
    {
        status = calculator_from_rational_status(bigrational_get_denominator(denominator, operand));
    }
    negative = status == CALCULATOR_OK && bigint_is_negative(numerator);
    if (status == CALCULATOR_OK && negative && degree % 2U == 0U)
    {
        status = CALCULATOR_INVALID_ARGUMENT;
    }
    if (status == CALCULATOR_OK && negative)
    {
        status = calculator_from_integer_status(bigint_abs(numerator, numerator));
    }
    if (status == CALCULATOR_OK)
    {
        status = calculator_exact_integer_root(top, numerator, degree);
    }
    if (status == CALCULATOR_OK)
    {
        status = calculator_exact_integer_root(bottom, denominator, degree);
    }
    if (status == CALCULATOR_OK && negative)
    {
        status = calculator_from_integer_status(bigint_negate(top, top));
    }
    if (status == CALCULATOR_OK)
    {
        status = calculator_from_rational_status(bigrational_set_fraction(result, top, bottom));
    }
done:
    bigint_destroy(numerator);
    bigint_destroy(denominator);
    bigint_destroy(top);
    bigint_destroy(bottom);
    return status;
}

CalculatorStatus calculator_exact_integer_operation(BigRational *result, BigRational *const *items, size_t count,
                                                           CalculatorFunctionImplementation function)
{
    BigInt *arguments[2] = {bigint_create(), bigint_create()};
    BigInt *denominator = bigint_create();
    BigInt *output = bigint_create();
    BigInt *limit = NULL;
    CalculatorStatus status = CALCULATOR_OUT_OF_MEMORY;

    if (arguments[0] == NULL || arguments[1] == NULL || denominator == NULL || output == NULL)
    {
        goto done;
    }
    for (size_t index = 0U; index < count; index++)
    {
        status = calculator_from_rational_status(bigrational_get_denominator(denominator, items[index]));
        if (status != CALCULATOR_OK)
        {
            goto done;
        }
        if (!bigint_is_one(denominator))
        {
            status = CALCULATOR_INVALID_ARGUMENT;
            goto done;
        }
        status = calculator_from_rational_status(bigrational_get_numerator(arguments[index], items[index]));
        if (status != CALCULATOR_OK)
        {
            goto done;
        }
    }
    if (function == CALCULATOR_FUNCTION_FACTORIAL)
    {
        limit = bigint_create();
        status = limit == NULL ? CALCULATOR_OUT_OF_MEMORY
                               : calculator_from_integer_status(bigint_set_string(limit, "10000"));
        if (status != CALCULATOR_OK)
        {
            goto done;
        }
        if (bigint_compare(arguments[0], limit) > 0)
        {
            status = CALCULATOR_VALUE_TOO_LARGE;
            goto done;
        }
    }
    switch (function)
    {
        case CALCULATOR_FUNCTION_FACTORIAL:
            status = calculator_from_integer_status(bigint_factorial(output, arguments[0]));
            break;
        case CALCULATOR_FUNCTION_ISQRT:
            status = calculator_from_integer_status(bigint_isqrt(output, arguments[0]));
            break;
        case CALCULATOR_FUNCTION_GCD:
            status = calculator_from_integer_status(bigint_gcd(output, arguments[0], arguments[1]));
            break;
        case CALCULATOR_FUNCTION_LCM:
            status = calculator_from_integer_status(bigint_lcm(output, arguments[0], arguments[1]));
            break;
        case CALCULATOR_FUNCTION_MOD:
            status = calculator_from_integer_status(bigint_mod(output, arguments[0], arguments[1]));
            break;
        case CALCULATOR_FUNCTION_NPR:
            status = calculator_from_integer_status(bigint_permutation(output, arguments[0], arguments[1]));
            break;
        case CALCULATOR_FUNCTION_NCR:
            status = calculator_from_integer_status(bigint_combination(output, arguments[0], arguments[1]));
            break;
        default:
            status = CALCULATOR_NOT_IMPLEMENTED;
            break;
    }
    if (status == CALCULATOR_OK)
    {
        status = calculator_from_rational_status(bigrational_from_bigint(result, output));
    }
done:
    bigint_destroy(arguments[0]);
    bigint_destroy(arguments[1]);
    bigint_destroy(denominator);
    bigint_destroy(output);
    bigint_destroy(limit);
    return status;
}

CalculatorStatus calculator_exact_rounding(BigRational *result, BigRational *const *items, size_t count,
                                                  CalculatorFunctionImplementation function)
{
    BigInt *numerator = bigint_create();
    BigInt *denominator = bigint_create();
    BigInt *quotient = bigint_create();
    BigInt *remainder = bigint_create();
    BigInt *power = bigint_create();
    BigInt *base = bigint_create();
    BigInt *exponent = bigint_create();
    BigInt *temporary = bigint_create();
    BigInt *one = bigint_create();
    BigInt *two = bigint_create();
    CalculatorStatus status = CALCULATOR_OUT_OF_MEMORY;
    int64_t places = 0;
    char digits[32];
    char *place_text = NULL;
    bool negative;

    if (numerator == NULL || denominator == NULL || quotient == NULL || remainder == NULL || power == NULL ||
        base == NULL || exponent == NULL || temporary == NULL || one == NULL || two == NULL)
    {
        goto done;
    }
    status = calculator_from_rational_status(bigrational_get_numerator(numerator, items[0]));
    if (status == CALCULATOR_OK)
    {
        status = calculator_from_rational_status(bigrational_get_denominator(denominator, items[0]));
    }
    if (status == CALCULATOR_OK && count == 2U)
    {
        status = calculator_from_rational_status(bigrational_get_denominator(temporary, items[1]));
        if (status == CALCULATOR_OK && !bigint_is_one(temporary))
        {
            status = CALCULATOR_INVALID_ARGUMENT;
        }
        if (status == CALCULATOR_OK)
        {
            status = calculator_from_rational_status(bigrational_get_numerator(temporary, items[1]));
        }
        if (status == CALCULATOR_OK)
        {
            place_text = bigint_to_string(temporary);
            status = place_text == NULL ? CALCULATOR_OUT_OF_MEMORY : CALCULATOR_OK;
        }
        if (status == CALCULATOR_OK)
        {
            errno = 0;
            intmax_t parsed = strtoimax(place_text, NULL, 10);
            status = errno == ERANGE || parsed < INT64_MIN || parsed > INT64_MAX ? CALCULATOR_VALUE_TOO_LARGE
                                                                                 : CALCULATOR_OK;
            if (status == CALCULATOR_OK)
            {
                places = (int64_t)parsed;
            }
        }
    }
    if (status != CALCULATOR_OK)
    {
        goto done;
    }
    if (places < -CALCULATOR_MAX_OUTPUT_SCALE || places > CALCULATOR_MAX_OUTPUT_SCALE)
    {
        status = CALCULATOR_NOT_IMPLEMENTED;
        goto done;
    }
    negative = bigint_is_negative(numerator);
    status = calculator_from_integer_status(bigint_set_string(one, "1"));
    if (status == CALCULATOR_OK)
    {
        status = calculator_from_integer_status(bigint_set_string(two, "2"));
    }
    if (status == CALCULATOR_OK)
    {
        status = calculator_from_integer_status(bigint_set_string(base, "10"));
    }
    if (status == CALCULATOR_OK)
    {
        (void)snprintf(digits, sizeof(digits), "%lld", (long long)(places < 0 ? -places : places));
        status = calculator_from_integer_status(bigint_set_string(exponent, digits));
    }
    if (status == CALCULATOR_OK)
    {
        status = calculator_from_integer_status(bigint_pow(power, base, exponent));
    }
    if (status == CALCULATOR_OK && places > 0)
    {
        status = calculator_from_integer_status(bigint_mul(numerator, numerator, power));
    }
    if (status == CALCULATOR_OK && places < 0)
    {
        status = calculator_from_integer_status(bigint_mul(denominator, denominator, power));
    }
    if (status == CALCULATOR_OK)
    {
        status = calculator_from_integer_status(bigint_div_mod(quotient, remainder, numerator, denominator));
    }
    if (status == CALCULATOR_OK && !bigint_is_zero(remainder))
    {
        bool adjust = false;
        if (function == CALCULATOR_FUNCTION_FLOOR)
        {
            adjust = negative;
        }
        else if (function == CALCULATOR_FUNCTION_CEIL)
        {
            adjust = !negative;
        }
        else if (function == CALCULATOR_FUNCTION_ROUND)
        {
            status = calculator_from_integer_status(bigint_abs(temporary, remainder));
            if (status == CALCULATOR_OK)
            {
                status = calculator_from_integer_status(bigint_mul(temporary, temporary, two));
            }
            if (status == CALCULATOR_OK)
            {
                int comparison = bigint_compare(temporary, denominator);
                if (comparison > 0)
                {
                    adjust = true;
                }
                else if (comparison == 0)
                {
                    status = calculator_from_integer_status(bigint_mod(temporary, quotient, two));
                    adjust = status == CALCULATOR_OK && !bigint_is_zero(temporary);
                }
            }
        }
        if (status == CALCULATOR_OK && adjust)
        {
            status = calculator_from_integer_status(negative ? bigint_sub(quotient, quotient, one)
                                                             : bigint_add(quotient, quotient, one));
        }
    }
    if (status == CALCULATOR_OK && places > 0)
    {
        status = calculator_from_rational_status(bigrational_set_fraction(result, quotient, power));
    }
    else if (status == CALCULATOR_OK)
    {
        if (places < 0)
        {
            status = calculator_from_integer_status(bigint_mul(quotient, quotient, power));
        }
        if (status == CALCULATOR_OK)
        {
            status = calculator_from_rational_status(bigrational_from_bigint(result, quotient));
        }
    }
done:
    free(place_text);
    bigint_destroy(numerator);
    bigint_destroy(denominator);
    bigint_destroy(quotient);
    bigint_destroy(remainder);
    bigint_destroy(power);
    bigint_destroy(base);
    bigint_destroy(exponent);
    bigint_destroy(temporary);
    bigint_destroy(one);
    bigint_destroy(two);
    return status;
}
