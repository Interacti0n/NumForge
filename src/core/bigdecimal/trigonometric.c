#include <numforge/bigdecimal.h>
#include "src/core/bigdecimal/bigdecimal_internal.h"
#include "src/runtime/numforge_alloc.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * Radian trigonometric functions. Forward functions reduce by pi/2 and
 * evaluate the requested sine/cosine series on [-pi/4, pi/4]. Inverse functions use
 * a guarded arctangent series after reciprocal and half-angle reduction.
 */

#define BIGDECIMAL_TRIG_GUARD_DIGITS 24
#define BIGDECIMAL_TRIG_REDUCTION_GUARD_DIGITS 8
#define BIGDECIMAL_ATAN_HALF_ANGLES 8U

#define TRIG_TRY(operation)                  \
    do                                       \
    {                                        \
        status = (operation);                \
        if (status != BIGDECIMAL_OK)         \
        {                                    \
            goto cleanup;                    \
        }                                    \
    } while (0)

static BigDecimalStatus trig_binary_rounded(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b,
    int64_t digits,
    bool subtract
)
{
    /* Outputs are private work values. Public callers publish only after the
     * complete operation succeeds, so no extra transaction object is needed. */
    BigDecimalStatus status = subtract ? bigdecimal_sub(result, a, b) : bigdecimal_add(result, a, b);
    if (status == BIGDECIMAL_OK)
        status = bigdecimal_round_significant(result, result, digits, BIGDECIMAL_ROUND_HALF_EVEN);
    return status;
}

static BigDecimalStatus trig_multiply_rounded(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b,
    int64_t digits
)
{
    BigDecimalStatus status = bigdecimal_mul(result, a, b);
    if (status == BIGDECIMAL_OK)
        status = bigdecimal_round_significant(result, result, digits, BIGDECIMAL_ROUND_HALF_EVEN);
    return status;
}

static BigDecimalStatus trig_work_digits(
    int64_t digits,
    int64_t *work_digits
)
{
    if (digits < 1)
    {
        return BIGDECIMAL_INVALID_ARGUMENT;
    }

    if (!bigdecimal_i64_add(digits, BIGDECIMAL_TRIG_GUARD_DIGITS, work_digits))
    {
        return BIGDECIMAL_VALUE_TOO_LARGE;
    }

    return BIGDECIMAL_OK;
}

static BigDecimalStatus trig_reduction_digits(
    const BigDecimal *value,
    int64_t work_digits,
    int64_t *reduction_digits
)
{
    char *coefficient = bigint_to_string(value->coefficient);
    const char *number;
    size_t length;
    uint64_t integer_digits = 0U;
    uint64_t scale_magnitude;
    int64_t extra;

    if (coefficient == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    number = coefficient[0] == '-' ? coefficient + 1 : coefficient;
    length = strlen(number);

    if (value->scale >= 0)
    {
        uint64_t scale = (uint64_t)value->scale;
        integer_digits = (uint64_t)length > scale ? (uint64_t)length - scale : 0U;
    }
    else
    {
        scale_magnitude = bigdecimal_abs_i64(value->scale);

        if ((uint64_t)length > UINT64_MAX - scale_magnitude)
        {
            free(coefficient);
            return BIGDECIMAL_VALUE_TOO_LARGE;
        }

        integer_digits = (uint64_t)length + scale_magnitude;
    }

    free(coefficient);

    if (integer_digits > (uint64_t)INT64_MAX - BIGDECIMAL_TRIG_REDUCTION_GUARD_DIGITS)
    {
        return BIGDECIMAL_VALUE_TOO_LARGE;
    }

    extra = (int64_t)integer_digits + BIGDECIMAL_TRIG_REDUCTION_GUARD_DIGITS;

    if (!bigdecimal_i64_add(work_digits, extra, reduction_digits))
    {
        return BIGDECIMAL_VALUE_TOO_LARGE;
    }

    return BIGDECIMAL_OK;
}

static BigDecimalStatus trig_quadrant(
    const BigDecimal *quotient,
    unsigned *quadrant
)
{
    BigInt *integer = bigint_create();
    BigInt *four = bigint_create();
    BigInt *remainder = bigint_create();
    BigDecimalStatus status = BIGDECIMAL_OUT_OF_MEMORY;
    char *text = NULL;
    long parsed;

    if (integer == NULL || four == NULL || remainder == NULL)
    {
        goto cleanup;
    }

    TRIG_TRY(bigdecimal_to_bigint(integer, quotient));
    status = bigdecimal_from_bigint_status(bigint_set_string(four, "4"));

    if (status != BIGDECIMAL_OK)
    {
        goto cleanup;
    }

    status = bigdecimal_from_bigint_status(bigint_mod(remainder, integer, four));

    if (status != BIGDECIMAL_OK)
    {
        goto cleanup;
    }

    text = bigint_to_string(remainder);

    if (text == NULL)
    {
        status = BIGDECIMAL_OUT_OF_MEMORY;
        goto cleanup;
    }

    parsed = strtol(text, NULL, 10);

    if (parsed < 0)
    {
        parsed += 4;
    }

    *quadrant = (unsigned)parsed;
    status = BIGDECIMAL_OK;

cleanup:
    free(text);
    bigint_destroy(integer);
    bigint_destroy(four);
    bigint_destroy(remainder);
    return status;
}

static BigDecimalStatus trig_reduce(
    BigDecimal *remainder,
    unsigned *quadrant,
    const BigDecimal *value,
    int64_t work_digits,
    bool adaptive
)
{
    BigDecimal *pi = bigdecimal_create();
    BigDecimal *two = bigdecimal_create();
    BigDecimal *half_pi = bigdecimal_create();
    BigDecimal *quotient = bigdecimal_create();
    BigDecimal *product = bigdecimal_create();
    BigDecimal *difference = bigdecimal_create();
    BigDecimalStatus status = BIGDECIMAL_OUT_OF_MEMORY;
    int64_t reduction_digits;
    int64_t base_reduction_digits;

    if (pi == NULL || two == NULL || half_pi == NULL || quotient == NULL ||
        product == NULL || difference == NULL)
    {
        goto cleanup;
    }

    TRIG_TRY(trig_reduction_digits(value, work_digits, &reduction_digits));
    base_reduction_digits = reduction_digits;
    TRIG_TRY(bigdecimal_set_string(two, "2"));
    for (;;)
    {
        int64_t required_digits;
        int64_t lost_digits;
        char *coefficient;
        size_t length;

        TRIG_TRY(bigdecimal_set_constant_significant(
            pi, BIGDECIMAL_CONSTANT_PI, reduction_digits, BIGDECIMAL_ROUND_HALF_EVEN));
        TRIG_TRY(bigdecimal_div_exact_or_significant(
            half_pi, pi, two, reduction_digits, BIGDECIMAL_ROUND_HALF_EVEN));
        TRIG_TRY(bigdecimal_div(
            quotient, value, half_pi, 0, BIGDECIMAL_ROUND_HALF_EVEN));
        TRIG_TRY(bigdecimal_mul(product, quotient, half_pi));
        TRIG_TRY(bigdecimal_sub(difference, value, product));

        /* A zero quotient introduces no pi error (including tiny inputs).
         * Otherwise cancellation needs additional absolute pi precision so
         * the reduced angle retains work_digits significant digits. This
         * protects tangent near odd multiples of pi/2. */
        if (!adaptive || bigint_is_zero(quotient->coefficient))
        {
            break;
        }
        if (bigint_is_zero(difference->coefficient))
        {
            /* A finite decimal cannot equal a nonzero irrational multiple
             * of pi/2. Refine instead of treating rounded pi as a pole. */
            if (!bigdecimal_i64_add(reduction_digits, reduction_digits, &required_digits))
            {
                status = BIGDECIMAL_VALUE_TOO_LARGE;
                goto cleanup;
            }
        }
        else
        {
            coefficient = bigint_to_string(difference->coefficient);
            if (coefficient == NULL)
            {
                status = BIGDECIMAL_OUT_OF_MEMORY;
                goto cleanup;
            }
            length = strlen(coefficient + (coefficient[0] == '-'));
            free(coefficient);
#if SIZE_MAX > INT64_MAX
            if (length > (size_t)INT64_MAX)
            {
                status = BIGDECIMAL_VALUE_TOO_LARGE;
                goto cleanup;
            }
#endif
            if (!bigdecimal_i64_sub(difference->scale, (int64_t)length, &lost_digits) ||
                !bigdecimal_i64_add(base_reduction_digits,
                    lost_digits > 0 ? lost_digits : 0, &required_digits))
            {
                status = BIGDECIMAL_VALUE_TOO_LARGE;
                goto cleanup;
            }
        }
        if (required_digits <= reduction_digits)
        {
            break;
        }
        reduction_digits = required_digits;
    }
    TRIG_TRY(trig_quadrant(quotient, quadrant));
    TRIG_TRY(bigdecimal_round_significant(
        remainder, difference, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));

cleanup:
    bigdecimal_destroy(pi);
    bigdecimal_destroy(two);
    bigdecimal_destroy(half_pi);
    bigdecimal_destroy(quotient);
    bigdecimal_destroy(product);
    bigdecimal_destroy(difference);
    return status;
}

static BigDecimalStatus trig_series(
    BigDecimal *sine,
    BigDecimal *cosine,
    const BigDecimal *value,
    int64_t digits
)
{
    BigDecimal *square = bigdecimal_create();
    BigDecimal *sin_term = sine != NULL ? bigdecimal_create() : NULL;
    BigDecimal *cos_term = cosine != NULL ? bigdecimal_create() : NULL;
    BigDecimal *next_sine = sine != NULL ? bigdecimal_create() : NULL;
    BigDecimal *next_cosine = cosine != NULL ? bigdecimal_create() : NULL;
    BigDecimal *denominator = bigdecimal_create();
    BigDecimal *one = cosine != NULL ? bigdecimal_create() : NULL;
    BigDecimalWorkspace workspace = {0};
    BigDecimalStatus status = BIGDECIMAL_OUT_OF_MEMORY;
    bool sine_done = sine == NULL;
    bool cosine_done = cosine == NULL;
    int comparison;

    if (square == NULL || denominator == NULL ||
        (sine != NULL && (sin_term == NULL || next_sine == NULL)) ||
        (cosine != NULL && (cos_term == NULL || next_cosine == NULL || one == NULL)))
    {
        goto cleanup;
    }

    if (cosine != NULL) { TRIG_TRY(bigdecimal_set_string(one, "1")); }
    TRIG_TRY(bigdecimal_mul_significant_workspace(square, value, value, digits,
        BIGDECIMAL_ROUND_HALF_EVEN, &workspace));
    if (sine != NULL) { TRIG_TRY(bigdecimal_copy(sin_term, value)); TRIG_TRY(bigdecimal_copy(sine, value)); }
    if (cosine != NULL) { TRIG_TRY(bigdecimal_copy(cos_term, one)); TRIG_TRY(bigdecimal_copy(cosine, one)); }

    for (uint64_t iteration = 1U; iteration <= (uint64_t)digits + 32U; iteration++)
    {
        uint64_t twice;
        uint64_t sin_denominator;
        uint64_t cos_denominator;

        if (!numforge_budget_check())
        {
            status = BIGDECIMAL_OUT_OF_MEMORY;
            goto cleanup;
        }

        if (iteration > (UINT64_MAX - 1U) / 2U)
        {
            status = BIGDECIMAL_VALUE_TOO_LARGE;
            goto cleanup;
        }

        twice = iteration * 2U;

        if (twice > UINT64_MAX / (twice + 1U) ||
            (twice - 1U) > UINT64_MAX / twice)
        {
            status = BIGDECIMAL_VALUE_TOO_LARGE;
            goto cleanup;
        }

        sin_denominator = twice * (twice + 1U);
        cos_denominator = (twice - 1U) * twice;

        if (!sine_done) {
            TRIG_TRY(bigdecimal_mul_significant_workspace(sin_term, sin_term, square,
                digits, BIGDECIMAL_ROUND_HALF_EVEN, &workspace));
            TRIG_TRY(bigdecimal_negate(sin_term, sin_term));
            TRIG_TRY(bigdecimal_set_u64(denominator, sin_denominator));
            TRIG_TRY(bigdecimal_div_significant_workspace(sin_term, sin_term, denominator,
                digits, BIGDECIMAL_ROUND_HALF_EVEN, &workspace));
            TRIG_TRY(bigdecimal_add_significant_workspace(next_sine, sine, sin_term,
                digits, BIGDECIMAL_ROUND_HALF_EVEN, &workspace));
            TRIG_TRY(bigdecimal_compare(&comparison, next_sine, sine));
            sine_done = comparison == 0;
            /* Both values are private and independently owned. Swap their
             * coefficients instead of allocating another transaction copy. */
            BigDecimal swap = *sine;*sine = *next_sine;*next_sine = swap;
        }

        if (!cosine_done) {
            TRIG_TRY(bigdecimal_mul_significant_workspace(cos_term, cos_term, square,
                digits, BIGDECIMAL_ROUND_HALF_EVEN, &workspace));
            TRIG_TRY(bigdecimal_negate(cos_term, cos_term));
            TRIG_TRY(bigdecimal_set_u64(denominator, cos_denominator));
            TRIG_TRY(bigdecimal_div_significant_workspace(cos_term, cos_term, denominator,
                digits, BIGDECIMAL_ROUND_HALF_EVEN, &workspace));
            TRIG_TRY(bigdecimal_add_significant_workspace(next_cosine, cosine, cos_term,
                digits, BIGDECIMAL_ROUND_HALF_EVEN, &workspace));
            TRIG_TRY(bigdecimal_compare(&comparison, next_cosine, cosine));
            cosine_done = comparison == 0;
            BigDecimal swap = *cosine;*cosine = *next_cosine;*next_cosine = swap;
        }

        if (sine_done && cosine_done)
        {
            status = BIGDECIMAL_OK;
            goto cleanup;
        }
    }

    status = BIGDECIMAL_VALUE_TOO_LARGE;

cleanup:
    bigdecimal_workspace_clear(&workspace);
    bigdecimal_destroy(square);
    bigdecimal_destroy(sin_term);
    bigdecimal_destroy(cos_term);
    bigdecimal_destroy(next_sine);
    bigdecimal_destroy(next_cosine);
    bigdecimal_destroy(denominator);
    bigdecimal_destroy(one);
    return status;
}

static BigDecimalStatus calculate_sin_cos(
    BigDecimal *sine,
    BigDecimal *cosine,
    const BigDecimal *value,
    int64_t digits,
    bool adaptive
)
{
    BigDecimal *remainder = bigdecimal_create();
    BigDecimal *reduced_sine = NULL;
    BigDecimal *reduced_cosine = NULL;
    BigDecimalStatus status = BIGDECIMAL_OUT_OF_MEMORY;
    unsigned quadrant = 0U;

    if (remainder == NULL)
    {
        goto cleanup;
    }

    TRIG_TRY(trig_reduce(remainder, &quadrant, value, digits, adaptive));
    bool need_sine = (quadrant & 1U) == 0U ? sine != NULL : cosine != NULL;
    bool need_cosine = (quadrant & 1U) == 0U ? cosine != NULL : sine != NULL;
    if (need_sine) reduced_sine = bigdecimal_create();
    if (need_cosine) reduced_cosine = bigdecimal_create();
    if ((need_sine && reduced_sine == NULL) || (need_cosine && reduced_cosine == NULL)) { status = BIGDECIMAL_OUT_OF_MEMORY; goto cleanup; }
    TRIG_TRY(trig_series(reduced_sine, reduced_cosine, remainder, digits));

    switch (quadrant)
    {
        case 0U:
            if (sine != NULL) { TRIG_TRY(bigdecimal_copy(sine, reduced_sine)); }
            if (cosine != NULL) { TRIG_TRY(bigdecimal_copy(cosine, reduced_cosine)); }
            break;
        case 1U:
            if (sine != NULL) { TRIG_TRY(bigdecimal_copy(sine, reduced_cosine)); }
            if (cosine != NULL) { TRIG_TRY(bigdecimal_negate(cosine, reduced_sine)); }
            break;
        case 2U:
            if (sine != NULL) { TRIG_TRY(bigdecimal_negate(sine, reduced_sine)); }
            if (cosine != NULL) { TRIG_TRY(bigdecimal_negate(cosine, reduced_cosine)); }
            break;
        default:
            if (sine != NULL) { TRIG_TRY(bigdecimal_negate(sine, reduced_cosine)); }
            if (cosine != NULL) { TRIG_TRY(bigdecimal_copy(cosine, reduced_sine)); }
            break;
    }

cleanup:
    bigdecimal_destroy(remainder);
    bigdecimal_destroy(reduced_sine);
    bigdecimal_destroy(reduced_cosine);
    return status;
}

BigDecimalStatus bigdecimal_sin_cos_pair(
    BigDecimal *sine, BigDecimal *cosine, const BigDecimal *value,
    int64_t digits, BigDecimalRoundingMode rounding
)
{
    BigDecimal *s = NULL, *c = NULL;
    BigDecimalStatus status;
    int64_t work_digits;
    if (sine == NULL || cosine == NULL || value == NULL) return BIGDECIMAL_NULL_ARGUMENT;
    if (sine == cosine || !bigdecimal_valid_rounding(rounding)) return BIGDECIMAL_INVALID_ARGUMENT;
    status = trig_work_digits(digits, &work_digits);
    if (status != BIGDECIMAL_OK) return status;
    s = bigdecimal_create();
    c = bigdecimal_create();
    if (s == NULL || c == NULL)
    {
        status = BIGDECIMAL_OUT_OF_MEMORY;
        goto cleanup;
    }
    if (bigint_is_zero(value->coefficient))
    {
        TRIG_TRY(bigdecimal_set_string(s, "0"));
        TRIG_TRY(bigdecimal_set_string(c, "1"));
    }
    else
    {
        // Public sine/cosine use the same non-adaptive reduction and guards.
        TRIG_TRY(calculate_sin_cos(s, c, value, work_digits, false));
        TRIG_TRY(bigdecimal_round_significant(s, s, digits, rounding));
        TRIG_TRY(bigdecimal_round_significant(c, c, digits, rounding));
    }
    // All input reads and fallible work finish before either output changes.
    bigdecimal_commit(sine, s); s = NULL;
    bigdecimal_commit(cosine, c); c = NULL;
cleanup:
    bigdecimal_destroy(s);
    bigdecimal_destroy(c);
    return status;
}

static BigDecimalStatus trig_forward(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding,
    unsigned operation
)
{
    BigDecimal *sine = NULL;
    BigDecimal *cosine = NULL;
    BigDecimal *temporary = NULL;
    BigDecimalStatus status;
    int64_t work_digits;

    if (result == NULL || value == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    if (!bigdecimal_valid_rounding(rounding))
    {
        return BIGDECIMAL_INVALID_ARGUMENT;
    }

    status = trig_work_digits(digits, &work_digits);

    if (status != BIGDECIMAL_OK)
    {
        return status;
    }

    if (bigint_is_zero(value->coefficient))
    {
        return bigdecimal_set_string(result, operation == 1U ? "1" : "0");
    }

    if (operation != 1U) sine = bigdecimal_create();
    if (operation != 0U) cosine = bigdecimal_create();
    temporary = bigdecimal_create();

    if ((operation != 1U && sine == NULL) || (operation != 0U && cosine == NULL) || temporary == NULL)
    {
        status = BIGDECIMAL_OUT_OF_MEMORY;
        goto cleanup;
    }

    TRIG_TRY(calculate_sin_cos(sine, cosine, value, work_digits, operation == 2U));

    if (operation == 0U)
    {
        TRIG_TRY(bigdecimal_round_significant(temporary, sine, digits, rounding));
    }
    else if (operation == 1U)
    {
        TRIG_TRY(bigdecimal_round_significant(temporary, cosine, digits, rounding));
    }
    else
    {
        TRIG_TRY(bigdecimal_div_significant(temporary, sine, cosine, digits, rounding));
    }

    TRIG_TRY(bigdecimal_copy(result, temporary));

cleanup:
    bigdecimal_destroy(sine);
    bigdecimal_destroy(cosine);
    bigdecimal_destroy(temporary);
    return status;
}

BigDecimalStatus bigdecimal_sin(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    return trig_forward(result, value, digits, rounding, 0U);
}

BigDecimalStatus bigdecimal_cos(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    return trig_forward(result, value, digits, rounding, 1U);
}

BigDecimalStatus bigdecimal_tan(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    return trig_forward(result, value, digits, rounding, 2U);
}

static BigDecimalStatus calculate_atan(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits
)
{
    BigDecimal *z = bigdecimal_create();
    BigDecimal *one = bigdecimal_create();
    BigDecimal *square = bigdecimal_create();
    BigDecimal *root = bigdecimal_create();
    BigDecimal *denominator = bigdecimal_create();
    BigDecimal *term = bigdecimal_create();
    BigDecimal *fraction = bigdecimal_create();
    BigDecimal *sum = bigdecimal_create();
    BigDecimal *next = bigdecimal_create();
    BigDecimal *pi = bigdecimal_create();
    BigDecimal *half_pi = bigdecimal_create();
    BigDecimal *factor = bigdecimal_create();
    BigDecimalStatus status = BIGDECIMAL_OUT_OF_MEMORY;
    bool negative = false;
    bool reciprocal = false;
    bool converged = false;
    int comparison = 0;

    if (z == NULL || one == NULL || square == NULL || root == NULL || denominator == NULL ||
        term == NULL || fraction == NULL || sum == NULL || next == NULL || pi == NULL ||
        half_pi == NULL || factor == NULL)
    {
        goto cleanup;
    }

    TRIG_TRY(bigdecimal_set_string(one, "1"));
    TRIG_TRY(bigdecimal_sign(&comparison, value));
    negative = comparison < 0;
    TRIG_TRY(bigdecimal_abs(z, value));
    TRIG_TRY(bigdecimal_compare(&comparison, z, one));

    if (comparison > 0)
    {
        reciprocal = true;
        TRIG_TRY(bigdecimal_div_significant(
            z, one, z, digits, BIGDECIMAL_ROUND_HALF_EVEN));
    }

    for (unsigned reduction = 0U; reduction < BIGDECIMAL_ATAN_HALF_ANGLES; reduction++)
    {
        TRIG_TRY(trig_multiply_rounded(square, z, z, digits));
        TRIG_TRY(trig_binary_rounded(square, one, square, digits, false));
        TRIG_TRY(bigdecimal_sqrt(
            root, square, digits, BIGDECIMAL_ROUND_HALF_EVEN));
        TRIG_TRY(trig_binary_rounded(denominator, one, root, digits, false));
        TRIG_TRY(bigdecimal_div_significant(
            z, z, denominator, digits, BIGDECIMAL_ROUND_HALF_EVEN));
    }

    TRIG_TRY(trig_multiply_rounded(square, z, z, digits));
    TRIG_TRY(bigdecimal_copy(term, z));
    TRIG_TRY(bigdecimal_copy(sum, z));

    for (uint64_t iteration = 1U; iteration <= (uint64_t)digits + 32U; iteration++)
    {
        uint64_t divisor;

        if (!numforge_budget_check())
        {
            status = BIGDECIMAL_OUT_OF_MEMORY;
            goto cleanup;
        }

        if (iteration > (UINT64_MAX - 1U) / 2U)
        {
            status = BIGDECIMAL_VALUE_TOO_LARGE;
            goto cleanup;
        }

        divisor = iteration * 2U + 1U;
        TRIG_TRY(trig_multiply_rounded(term, term, square, digits));
        TRIG_TRY(bigdecimal_negate(term, term));
        TRIG_TRY(bigdecimal_set_u64(denominator, divisor));
        TRIG_TRY(bigdecimal_div_significant(
            fraction, term, denominator, digits, BIGDECIMAL_ROUND_HALF_EVEN));
        TRIG_TRY(trig_binary_rounded(next, sum, fraction, digits, false));
        TRIG_TRY(bigdecimal_compare(&comparison, next, sum));
        TRIG_TRY(bigdecimal_copy(sum, next));

        if (comparison == 0)
        {
            converged = true;
            break;
        }
    }

    if (!converged)
    {
        status = BIGDECIMAL_VALUE_TOO_LARGE;
        goto cleanup;
    }

    TRIG_TRY(bigdecimal_set_u64(factor, 1U << BIGDECIMAL_ATAN_HALF_ANGLES));
    TRIG_TRY(trig_multiply_rounded(sum, sum, factor, digits));

    if (reciprocal)
    {
        TRIG_TRY(bigdecimal_set_constant_significant(
            pi, BIGDECIMAL_CONSTANT_PI, digits, BIGDECIMAL_ROUND_HALF_EVEN));
        TRIG_TRY(bigdecimal_set_string(factor, "2"));
        TRIG_TRY(bigdecimal_div_exact_or_significant(
            half_pi, pi, factor, digits, BIGDECIMAL_ROUND_HALF_EVEN));
        TRIG_TRY(trig_binary_rounded(sum, half_pi, sum, digits, true));
    }

    if (negative)
    {
        TRIG_TRY(bigdecimal_negate(sum, sum));
    }

    TRIG_TRY(bigdecimal_copy(result, sum));

cleanup:
    bigdecimal_destroy(z);
    bigdecimal_destroy(one);
    bigdecimal_destroy(square);
    bigdecimal_destroy(root);
    bigdecimal_destroy(denominator);
    bigdecimal_destroy(term);
    bigdecimal_destroy(fraction);
    bigdecimal_destroy(sum);
    bigdecimal_destroy(next);
    bigdecimal_destroy(pi);
    bigdecimal_destroy(half_pi);
    bigdecimal_destroy(factor);
    return status;
}

BigDecimalStatus bigdecimal_atan(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    BigDecimal *temporary;
    BigDecimalStatus status;
    int64_t work_digits;

    if (result == NULL || value == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    if (!bigdecimal_valid_rounding(rounding))
    {
        return BIGDECIMAL_INVALID_ARGUMENT;
    }

    status = trig_work_digits(digits, &work_digits);

    if (status != BIGDECIMAL_OK)
    {
        return status;
    }

    if (bigint_is_zero(value->coefficient))
    {
        return bigdecimal_set_string(result, "0");
    }

    temporary = bigdecimal_create();

    if (temporary == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    status = calculate_atan(temporary, value, work_digits);

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_round_significant(temporary, temporary, digits, rounding);
    }

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_copy(result, temporary);
    }

    bigdecimal_destroy(temporary);
    return status;
}

static BigDecimalStatus trig_inverse(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding,
    bool arccosine
)
{
    BigDecimal *one = NULL;
    BigDecimal *negative_one = NULL;
    BigDecimal *square = NULL;
    BigDecimal *radicand = NULL;
    BigDecimal *root = NULL;
    BigDecimal *ratio = NULL;
    BigDecimal *angle = NULL;
    BigDecimal *pi = NULL;
    BigDecimal *two = NULL;
    BigDecimal *half_pi = NULL;
    BigDecimalStatus status;
    int64_t work_digits;
    int comparison_low;
    int comparison_high;

    if (result == NULL || value == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    if (!bigdecimal_valid_rounding(rounding))
    {
        return BIGDECIMAL_INVALID_ARGUMENT;
    }

    status = trig_work_digits(digits, &work_digits);

    if (status != BIGDECIMAL_OK)
    {
        return status;
    }

    one = bigdecimal_create();
    negative_one = bigdecimal_create();
    square = bigdecimal_create();
    radicand = bigdecimal_create();
    root = bigdecimal_create();
    ratio = bigdecimal_create();
    angle = bigdecimal_create();
    pi = bigdecimal_create();
    two = bigdecimal_create();
    half_pi = bigdecimal_create();
    status = BIGDECIMAL_OUT_OF_MEMORY;

    if (one == NULL || negative_one == NULL || square == NULL || radicand == NULL || root == NULL ||
        ratio == NULL || angle == NULL || pi == NULL || two == NULL || half_pi == NULL)
    {
        goto cleanup;
    }

    TRIG_TRY(bigdecimal_set_string(one, "1"));
    TRIG_TRY(bigdecimal_set_string(negative_one, "-1"));
    TRIG_TRY(bigdecimal_compare(&comparison_low, value, negative_one));
    TRIG_TRY(bigdecimal_compare(&comparison_high, value, one));

    if (comparison_low < 0 || comparison_high > 0)
    {
        status = BIGDECIMAL_INVALID_ARGUMENT;
        goto cleanup;
    }

    TRIG_TRY(bigdecimal_set_constant_significant(
        pi, BIGDECIMAL_CONSTANT_PI, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));
    TRIG_TRY(bigdecimal_set_string(two, "2"));
    TRIG_TRY(bigdecimal_div_exact_or_significant(
        half_pi, pi, two, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));

    if (comparison_high == 0)
    {
        TRIG_TRY(bigdecimal_copy(angle, half_pi));
    }
    else if (comparison_low == 0)
    {
        TRIG_TRY(bigdecimal_negate(angle, half_pi));
    }
    else
    {
        /* Preserve the tiny distance from either endpoint before rounding. */
        TRIG_TRY(bigdecimal_sub(square, one, value));
        TRIG_TRY(bigdecimal_add(radicand, one, value));
        TRIG_TRY(bigdecimal_mul(radicand, radicand, square));
        TRIG_TRY(bigdecimal_sqrt(
            root, radicand, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));
        if (arccosine && !bigint_is_zero(value->coefficient))
        {
            int sign;

            TRIG_TRY(bigdecimal_sign(&sign, value));
            TRIG_TRY(bigdecimal_abs(square, value));
            TRIG_TRY(bigdecimal_div_significant(
                ratio, root, square, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));
            TRIG_TRY(calculate_atan(angle, ratio, work_digits));

            if (sign < 0)
            {
                TRIG_TRY(trig_binary_rounded(angle, pi, angle, work_digits, true));
            }

            arccosine = false;
        }
        else
        {
            TRIG_TRY(bigdecimal_div_significant(
                ratio, value, root, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));
            TRIG_TRY(calculate_atan(angle, ratio, work_digits));
        }
    }

    if (arccosine)
    {
        TRIG_TRY(trig_binary_rounded(angle, half_pi, angle, work_digits, true));
    }

    TRIG_TRY(bigdecimal_round_significant(angle, angle, digits, rounding));
    TRIG_TRY(bigdecimal_copy(result, angle));

cleanup:
    bigdecimal_destroy(one);
    bigdecimal_destroy(negative_one);
    bigdecimal_destroy(square);
    bigdecimal_destroy(radicand);
    bigdecimal_destroy(root);
    bigdecimal_destroy(ratio);
    bigdecimal_destroy(angle);
    bigdecimal_destroy(pi);
    bigdecimal_destroy(two);
    bigdecimal_destroy(half_pi);
    return status;
}

BigDecimalStatus bigdecimal_asin(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    return trig_inverse(result, value, digits, rounding, false);
}

BigDecimalStatus bigdecimal_acos(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    return trig_inverse(result, value, digits, rounding, true);
}

#undef TRIG_TRY
