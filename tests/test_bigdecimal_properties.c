#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unity.h>

#include <numforge/bigdecimal.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Deterministic generated tests for the public BigDecimal contract.

    The reference model deliberately uses only bounded int64_t coefficients and
    scales. It is independent from BigDecimal's BigInt implementation while
    remaining portable to every C17 compiler supported by NumForge.
------------------------------------------------------------------------------------------------------------------------------
*/

#define BIGDECIMAL_PROPERTY_ITERATIONS 384U

static uint64_t random_state = UINT64_C(0xD3C1A1A15EED1234);

static uint64_t next_random_u64(void)
{
    uint64_t value = random_state;

    value ^= value >> 12U;
    value ^= value << 25U;
    value ^= value >> 27U;
    random_state = value;

    return value * UINT64_C(2685821657736338717);
}

static int64_t random_coefficient(bool non_zero)
{
    int64_t value = (int64_t)(next_random_u64() % UINT64_C(2000001)) - INT64_C(1000000);

    if (non_zero && value == 0)
    {
        return 1;
    }
    return value;
}

static int64_t random_small_non_zero_coefficient(void)
{
    int64_t value = (int64_t)(next_random_u64() % UINT64_C(20001)) - INT64_C(10000);

    return value == 0 ? 1 : value;
}

static int64_t random_scale(void)
{
    return (int64_t)(next_random_u64() % UINT64_C(9)) - 4;
}

static int64_t pow10_i64(unsigned int exponent)
{
    int64_t value = 1;

    while (exponent > 0U)
    {
        value *= 10;
        exponent--;
    }
    return value;
}

static int64_t abs_i64(int64_t value)
{
    return value < 0 ? -value : value;
}

static void format_reference_decimal(char *output, size_t capacity, int64_t coefficient, int64_t scale)
{
    char digits[32];
    size_t digit_count;
    size_t offset = 0U;
    bool negative = coefficient < 0;
    int64_t magnitude;

    while (coefficient != 0 && coefficient % 10 == 0)
    {
        coefficient /= 10;
        scale--;
    }
    if (coefficient == 0)
    {
        (void)snprintf(output, capacity, "0");
        return;
    }

    magnitude = abs_i64(coefficient);
    (void)snprintf(digits, sizeof(digits), "%lld", (long long)magnitude);
    digit_count = strlen(digits);
    if (negative)
    {
        output[offset++] = '-';
    }

    if (scale <= 0)
    {
        memcpy(output + offset, digits, digit_count);
        offset += digit_count;
        while (scale < 0)
        {
            output[offset++] = '0';
            scale++;
        }
    }
    else if ((uint64_t)scale >= (uint64_t)digit_count)
    {
        output[offset++] = '0';
        output[offset++] = '.';
        while ((uint64_t)scale > (uint64_t)digit_count)
        {
            output[offset++] = '0';
            scale--;
        }
        memcpy(output + offset, digits, digit_count);
        offset += digit_count;
    }
    else
    {
        size_t whole_digits = digit_count - (size_t)scale;

        memcpy(output + offset, digits, whole_digits);
        offset += whole_digits;
        output[offset++] = '.';
        memcpy(output + offset, digits + whole_digits, digit_count - whole_digits);
        offset += digit_count - whole_digits;
    }
    output[offset] = '\0';
}

static BigDecimal *make_decimal_from_parts(int64_t coefficient, int64_t scale)
{
    BigDecimal *value = bigdecimal_create();
    char text[64];

    TEST_ASSERT_NOT_NULL(value);
    (void)snprintf(text, sizeof(text), "%lldE%+lld", (long long)coefficient, (long long)-scale);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, text));
    return value;
}

static BigDecimal *make_random_large_decimal(void)
{
    char text[160];
    size_t offset = 0U;
    size_t digit_count = (size_t)(next_random_u64() % UINT64_C(72)) + 8U;
    size_t decimal_after = (size_t)(next_random_u64() % (uint64_t)(digit_count - 1U)) + 1U;
    int exponent = (int)(next_random_u64() % UINT64_C(25)) - 12;
    BigDecimal *value = bigdecimal_create();

    TEST_ASSERT_NOT_NULL(value);
    if ((next_random_u64() & UINT64_C(1)) != 0)
    {
        text[offset++] = '-';
    }
    for (size_t index = 0U; index < digit_count; index++)
    {
        unsigned int digit = (unsigned int)(next_random_u64() % UINT64_C(10));

        if (index == 0U)
        {
            digit = (unsigned int)(next_random_u64() % UINT64_C(9)) + 1U;
        }
        text[offset++] = (char)('0' + digit);
        if (index + 1U == decimal_after)
        {
            text[offset++] = '.';
        }
    }
    for (size_t index = 0U; index < (size_t)(next_random_u64() % UINT64_C(5)); index++)
    {
        text[offset++] = '0';
    }
    (void)snprintf(text + offset, sizeof(text) - offset, "E%+d", exponent);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, text));
    return value;
}

static void assert_decimal_equals_reference(const BigDecimal *value, int64_t coefficient, int64_t scale)
{
    char expected[96];
    char *actual = NULL;

    format_reference_decimal(expected, sizeof(expected), coefficient, scale);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_to_string(value, &actual));
    TEST_ASSERT_NOT_NULL(actual);
    TEST_ASSERT_EQUAL_STRING(expected, actual);
    free(actual);
}

static int64_t reference_round_quotient(
    int64_t numerator,
    int64_t denominator,
    bool negative,
    BigDecimalRoundingMode rounding
)
{
    int64_t quotient = numerator / denominator;
    int64_t remainder = numerator % denominator;
    bool increment = false;

    switch (rounding)
    {
        case BIGDECIMAL_ROUND_TOWARD_ZERO:
            break;
        case BIGDECIMAL_ROUND_AWAY_FROM_ZERO:
            increment = remainder != 0;
            break;
        case BIGDECIMAL_ROUND_FLOOR:
            increment = negative && remainder != 0;
            break;
        case BIGDECIMAL_ROUND_CEILING:
            increment = !negative && remainder != 0;
            break;
        case BIGDECIMAL_ROUND_HALF_UP:
            increment = remainder >= (denominator + 1) / 2;
            break;
        case BIGDECIMAL_ROUND_HALF_EVEN:
            increment = remainder > denominator / 2 ||
                        (denominator % 2 == 0 && remainder == denominator / 2 && quotient % 2 != 0);
            break;
        default:
            TEST_FAIL_MESSAGE("Unexpected rounding mode in reference model");
    }

    return negative ? -(quotient + (increment ? 1 : 0)) : quotient + (increment ? 1 : 0);
}

static void reference_rescale(
    int64_t coefficient,
    int64_t scale,
    int64_t target_scale,
    BigDecimalRoundingMode rounding,
    int64_t *result_coefficient,
    int64_t *result_scale
)
{
    int64_t divisor;

    if (target_scale >= scale)
    {
        *result_coefficient = coefficient;
        *result_scale = scale;
        return;
    }

    divisor = pow10_i64((unsigned int)(scale - target_scale));
    *result_coefficient = reference_round_quotient(
        abs_i64(coefficient), divisor, coefficient < 0, rounding
    );
    *result_scale = target_scale;
}

static void reference_division(
    int64_t a_coefficient,
    int64_t a_scale,
    int64_t b_coefficient,
    int64_t b_scale,
    int64_t target_scale,
    BigDecimalRoundingMode rounding,
    int64_t *result_coefficient,
    int64_t *result_scale
)
{
    int64_t power = target_scale + b_scale - a_scale;
    int64_t numerator = abs_i64(a_coefficient);
    int64_t denominator = abs_i64(b_coefficient);
    bool negative = (a_coefficient < 0) != (b_coefficient < 0);

    if (power >= 0)
    {
        numerator *= pow10_i64((unsigned int)power);
    }
    else
    {
        denominator *= pow10_i64((unsigned int)-power);
    }

    *result_coefficient = reference_round_quotient(numerator, denominator, negative, rounding);
    *result_scale = target_scale;
}

void setUp(void)
{
    random_state = UINT64_C(0xD3C1A1A15EED1234);
}

void tearDown(void)
{
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Generated conversion, comparison, and exact-arithmetic properties.
------------------------------------------------------------------------------------------------------------------------------
*/
void test_property_parse_format_round_trip_and_canonical_form(void)
{
    for (size_t index = 0U; index < BIGDECIMAL_PROPERTY_ITERATIONS; index++)
    {
        int64_t coefficient = random_coefficient(false);
        int64_t scale = random_scale();
        BigDecimal *value = make_decimal_from_parts(coefficient, scale);
        BigDecimal *parsed = bigdecimal_create();
        char *text = NULL;
        int comparison = 0;

        TEST_ASSERT_NOT_NULL(parsed);
        assert_decimal_equals_reference(value, coefficient, scale);
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_to_string(value, &text));
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(parsed, text));
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_compare(&comparison, value, parsed));
        TEST_ASSERT_EQUAL_INT(0, comparison);

        free(text);
        bigdecimal_destroy(value);
        bigdecimal_destroy(parsed);
    }
}

void test_property_exact_arithmetic_matches_bounded_reference_model(void)
{
    for (size_t index = 0U; index < BIGDECIMAL_PROPERTY_ITERATIONS; index++)
    {
        int64_t a_coefficient = random_coefficient(false);
        int64_t b_coefficient = random_coefficient(false);
        int64_t a_scale = random_scale();
        int64_t b_scale = random_scale();
        int64_t common_scale = a_scale > b_scale ? a_scale : b_scale;
        int64_t aligned_a = a_coefficient * pow10_i64((unsigned int)(common_scale - a_scale));
        int64_t aligned_b = b_coefficient * pow10_i64((unsigned int)(common_scale - b_scale));
        BigDecimal *a = make_decimal_from_parts(a_coefficient, a_scale);
        BigDecimal *b = make_decimal_from_parts(b_coefficient, b_scale);
        BigDecimal *result = bigdecimal_create();
        BigDecimal *left_alias = bigdecimal_create();
        BigDecimal *right_alias = bigdecimal_create();

        TEST_ASSERT_NOT_NULL(result);
        TEST_ASSERT_NOT_NULL(left_alias);
        TEST_ASSERT_NOT_NULL(right_alias);

        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_add(result, a, b));
        assert_decimal_equals_reference(result, aligned_a + aligned_b, common_scale);
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_sub(result, a, b));
        assert_decimal_equals_reference(result, aligned_a - aligned_b, common_scale);
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_mul(result, a, b));
        assert_decimal_equals_reference(result, a_coefficient * b_coefficient, a_scale + b_scale);

        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_copy(left_alias, a));
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_add(left_alias, left_alias, b));
        assert_decimal_equals_reference(left_alias, aligned_a + aligned_b, common_scale);
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_copy(right_alias, b));
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_mul(right_alias, a, right_alias));
        assert_decimal_equals_reference(right_alias, a_coefficient * b_coefficient, a_scale + b_scale);

        bigdecimal_destroy(a);
        bigdecimal_destroy(b);
        bigdecimal_destroy(result);
        bigdecimal_destroy(left_alias);
        bigdecimal_destroy(right_alias);
    }
}

void test_property_large_coefficients_preserve_algebraic_invariants(void)
{
    for (size_t index = 0U; index < 96U; index++)
    {
        BigDecimal *a = make_random_large_decimal();
        BigDecimal *b = make_random_large_decimal();
        BigDecimal *parsed = bigdecimal_create();
        BigDecimal *sum = bigdecimal_create();
        BigDecimal *left_product = bigdecimal_create();
        BigDecimal *right_product = bigdecimal_create();
        char *text = NULL;
        int comparison = 0;

        TEST_ASSERT_NOT_NULL(parsed);
        TEST_ASSERT_NOT_NULL(sum);
        TEST_ASSERT_NOT_NULL(left_product);
        TEST_ASSERT_NOT_NULL(right_product);
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_to_string(a, &text));
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(parsed, text));
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_compare(&comparison, a, parsed));
        TEST_ASSERT_EQUAL_INT(0, comparison);

        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_add(sum, a, b));
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_sub(sum, sum, b));
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_compare(&comparison, a, sum));
        TEST_ASSERT_EQUAL_INT(0, comparison);
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_mul(left_product, a, b));
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_mul(right_product, b, a));
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_compare(&comparison, left_product, right_product));
        TEST_ASSERT_EQUAL_INT(0, comparison);

        free(text);
        bigdecimal_destroy(a);
        bigdecimal_destroy(b);
        bigdecimal_destroy(parsed);
        bigdecimal_destroy(sum);
        bigdecimal_destroy(left_product);
        bigdecimal_destroy(right_product);
    }
}

void test_property_comparison_negation_and_absolute_value(void)
{
    for (size_t index = 0U; index < BIGDECIMAL_PROPERTY_ITERATIONS; index++)
    {
        int64_t a_coefficient = random_coefficient(false);
        int64_t b_coefficient = random_coefficient(false);
        int64_t a_scale = random_scale();
        int64_t b_scale = random_scale();
        int64_t common_scale = a_scale > b_scale ? a_scale : b_scale;
        int64_t aligned_a = a_coefficient * pow10_i64((unsigned int)(common_scale - a_scale));
        int64_t aligned_b = b_coefficient * pow10_i64((unsigned int)(common_scale - b_scale));
        int comparison = 0;
        BigDecimal *a = make_decimal_from_parts(a_coefficient, a_scale);
        BigDecimal *b = make_decimal_from_parts(b_coefficient, b_scale);
        BigDecimal *value = bigdecimal_create();

        TEST_ASSERT_NOT_NULL(value);
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_compare(&comparison, a, b));
        TEST_ASSERT_EQUAL_INT(aligned_a < aligned_b ? -1 : aligned_a > aligned_b ? 1 : 0,
                              comparison < 0 ? -1 : comparison > 0 ? 1 : 0);

        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_copy(value, a));
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_negate(value, value));
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_negate(value, value));
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_compare(&comparison, value, a));
        TEST_ASSERT_EQUAL_INT(0, comparison);
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_abs(value, a));
        assert_decimal_equals_reference(value, abs_i64(a_coefficient), a_scale);

        bigdecimal_destroy(a);
        bigdecimal_destroy(b);
        bigdecimal_destroy(value);
    }
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Generated rounding and division properties.
------------------------------------------------------------------------------------------------------------------------------
*/
void test_property_rescale_matches_bounded_reference_model(void)
{
    const BigDecimalRoundingMode modes[] = {
        BIGDECIMAL_ROUND_TOWARD_ZERO,
        BIGDECIMAL_ROUND_AWAY_FROM_ZERO,
        BIGDECIMAL_ROUND_FLOOR,
        BIGDECIMAL_ROUND_CEILING,
        BIGDECIMAL_ROUND_HALF_UP,
        BIGDECIMAL_ROUND_HALF_EVEN
    };

    for (size_t index = 0U; index < BIGDECIMAL_PROPERTY_ITERATIONS; index++)
    {
        int64_t coefficient = random_coefficient(false);
        int64_t scale = random_scale();
        int64_t target_scale = random_scale();
        BigDecimal *value = make_decimal_from_parts(coefficient, scale);
        BigDecimal *result = bigdecimal_create();

        TEST_ASSERT_NOT_NULL(result);
        for (size_t mode = 0U; mode < sizeof(modes) / sizeof(modes[0]); mode++)
        {
            int64_t expected_coefficient;
            int64_t expected_scale;

            reference_rescale(coefficient, scale, target_scale, modes[mode],
                              &expected_coefficient, &expected_scale);
            TEST_ASSERT_EQUAL(BIGDECIMAL_OK,
                              bigdecimal_rescale(result, value, target_scale, modes[mode]));
            assert_decimal_equals_reference(result, expected_coefficient, expected_scale);
        }

        bigdecimal_destroy(value);
        bigdecimal_destroy(result);
    }
}

void test_property_division_matches_bounded_reference_model(void)
{
    const BigDecimalRoundingMode modes[] = {
        BIGDECIMAL_ROUND_TOWARD_ZERO,
        BIGDECIMAL_ROUND_AWAY_FROM_ZERO,
        BIGDECIMAL_ROUND_FLOOR,
        BIGDECIMAL_ROUND_CEILING,
        BIGDECIMAL_ROUND_HALF_UP,
        BIGDECIMAL_ROUND_HALF_EVEN
    };

    for (size_t index = 0U; index < BIGDECIMAL_PROPERTY_ITERATIONS; index++)
    {
        int64_t a_coefficient = random_small_non_zero_coefficient();
        int64_t b_coefficient = random_small_non_zero_coefficient();
        int64_t a_scale = random_scale();
        int64_t b_scale = random_scale();
        int64_t target_scale = random_scale();
        BigDecimal *a = make_decimal_from_parts(a_coefficient, a_scale);
        BigDecimal *b = make_decimal_from_parts(b_coefficient, b_scale);
        BigDecimal *result = bigdecimal_create();

        TEST_ASSERT_NOT_NULL(result);
        for (size_t mode = 0U; mode < sizeof(modes) / sizeof(modes[0]); mode++)
        {
            int64_t expected_coefficient;
            int64_t expected_scale;

            reference_division(a_coefficient, a_scale, b_coefficient, b_scale,
                               target_scale, modes[mode], &expected_coefficient, &expected_scale);
            TEST_ASSERT_EQUAL(BIGDECIMAL_OK,
                              bigdecimal_div(result, a, b, target_scale, modes[mode]));
            assert_decimal_equals_reference(result, expected_coefficient, expected_scale);
        }

        bigdecimal_destroy(a);
        bigdecimal_destroy(b);
        bigdecimal_destroy(result);
    }
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_property_parse_format_round_trip_and_canonical_form);
    RUN_TEST(test_property_exact_arithmetic_matches_bounded_reference_model);
    RUN_TEST(test_property_large_coefficients_preserve_algebraic_invariants);
    RUN_TEST(test_property_comparison_negation_and_absolute_value);
    RUN_TEST(test_property_rescale_matches_bounded_reference_model);
    RUN_TEST(test_property_division_matches_bounded_reference_model);

    return UNITY_END();
}
