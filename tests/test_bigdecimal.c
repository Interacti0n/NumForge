#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unity.h>

#include <numforge/bigdecimal.h>

void setUp(void)
{
}

void tearDown(void)
{
}

/* ============================================================
   Test helpers
   ============================================================ */

static BigDecimal *make_decimal(const char *text)
{
    BigDecimal *value = bigdecimal_create();

    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, text));
    return value;
}

static void assert_decimal_equals(const char *expected, const BigDecimal *value)
{
    char *actual = NULL;

    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_to_string(value, &actual));
    TEST_ASSERT_NOT_NULL(actual);
    TEST_ASSERT_EQUAL_STRING(expected, actual);
    free(actual);
}

static void assert_decimal_format(
    const char *expected,
    const char *input,
    int64_t places,
    BigDecimalRoundingMode rounding
)
{
    BigDecimal *value = make_decimal(input);
    char *actual = NULL;

    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,
                      bigdecimal_format(value, places, rounding, &actual));
    TEST_ASSERT_NOT_NULL(actual);
    TEST_ASSERT_EQUAL_STRING(expected, actual);
    free(actual);
    bigdecimal_destroy(value);
}

void test_lifetime_and_status_strings(void)
{
    BigDecimal *value = bigdecimal_create();

    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_STRING("success", bigdecimal_status_to_string(BIGDECIMAL_OK));
    TEST_ASSERT_EQUAL_STRING("unknown status", bigdecimal_status_to_string((BigDecimalStatus)999));
    assert_decimal_equals("0", value);

    bigdecimal_destroy(value);
    bigdecimal_destroy(NULL);
}

/* ============================================================
   Conversion and canonical form
   ============================================================ */

void test_parse_format_and_canonical_form(void)
{
    const char *input[] = { "1.2300", "12300e-4", ".5E+2", "-12.500e-1", "1e3", "-0.000" };
    const char *expected[] = { "1.23", "1.23", "50", "-1.25", "1000", "0" };

    for (size_t index = 0; index < sizeof(input) / sizeof(input[0]); index++)
    {
        BigDecimal *value = make_decimal(input[index]);
        assert_decimal_equals(expected[index], value);
        bigdecimal_destroy(value);
    }
}

void test_readable_format_thresholds_and_rounding_carry(void)
{
    assert_decimal_format("0.000000001", "1E-9", 10, BIGDECIMAL_ROUND_HALF_EVEN);
    assert_decimal_format("1E-10", "1E-10", 10, BIGDECIMAL_ROUND_HALF_EVEN);
    assert_decimal_format("1000000000", "999999999.5", 0, BIGDECIMAL_ROUND_HALF_UP);
    assert_decimal_format("1E+10", "9999999999.5", 0, BIGDECIMAL_ROUND_HALF_UP);
}

void test_invalid_parse_does_not_modify_destination(void)
{
    const char *invalid[] = { "", "+", ".", "1e", "1.2.3", " 1", "1x" };
    BigDecimal *value = make_decimal("7.5");

    for (size_t index = 0; index < sizeof(invalid) / sizeof(invalid[0]); index++)
    {
        TEST_ASSERT_EQUAL(BIGDECIMAL_INVALID_ARGUMENT, bigdecimal_set_string(value, invalid[index]));
        assert_decimal_equals("7.5", value);
    }

    bigdecimal_destroy(value);
}

void test_scale_overflow_does_not_modify_destination(void)
{
    BigDecimal *value = make_decimal("7.5");

    TEST_ASSERT_EQUAL(BIGDECIMAL_SCALE_OVERFLOW,
                      bigdecimal_set_string(value, "1e-9223372036854775808"));
    assert_decimal_equals("7.5", value);

    bigdecimal_destroy(value);
}

void test_copy_comparison_and_inspection(void)
{
    BigDecimal *a = make_decimal("-1.2");
    BigDecimal *b = make_decimal("-1.20");
    BigDecimal *copy = bigdecimal_create();
    bool result = false;
    int comparison = 0;

    TEST_ASSERT_NOT_NULL(copy);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_copy(copy, a));
    assert_decimal_equals("-1.2", copy);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_compare(&comparison, a, b));
    TEST_ASSERT_EQUAL_INT(0, comparison);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_is_negative(&result, copy));
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_is_zero(&result, copy));
    TEST_ASSERT_FALSE(result);

    bigdecimal_destroy(a);
    bigdecimal_destroy(b);
    bigdecimal_destroy(copy);
}

/* ============================================================
   Exact arithmetic
   ============================================================ */

void test_exact_arithmetic_and_aliasing(void)
{
    BigDecimal *a = make_decimal("1.2");
    BigDecimal *b = make_decimal("0.03");
    BigDecimal *negative = make_decimal("-1.5");
    BigDecimal *factor = make_decimal("2.4");
    BigDecimal *result = bigdecimal_create();

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_add(result, a, b));
    assert_decimal_equals("1.23", result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_sub(result, a, b));
    assert_decimal_equals("1.17", result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_mul(result, negative, factor));
    assert_decimal_equals("-3.6", result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_add(a, a, b));
    assert_decimal_equals("1.23", a);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_abs(result, negative));
    assert_decimal_equals("1.5", result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_negate(result, result));
    assert_decimal_equals("-1.5", result);

    bigdecimal_destroy(a);
    bigdecimal_destroy(b);
    bigdecimal_destroy(negative);
    bigdecimal_destroy(factor);
    bigdecimal_destroy(result);
}

void test_rescale_rounding(void)
{
    BigDecimal *value = make_decimal("1.250");
    BigDecimal *negative = make_decimal("-1.21");
    BigDecimal *result = bigdecimal_create();

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_rescale(result, value, 1, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_decimal_equals("1.2", result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_rescale(result, value, 1, BIGDECIMAL_ROUND_HALF_UP));
    assert_decimal_equals("1.3", result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_rescale(result, negative, 1, BIGDECIMAL_ROUND_FLOOR));
    assert_decimal_equals("-1.3", result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_rescale(result, negative, 1, BIGDECIMAL_ROUND_CEILING));
    assert_decimal_equals("-1.2", result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_rescale(result, value, 8, BIGDECIMAL_ROUND_TOWARD_ZERO));
    assert_decimal_equals("1.25", result);

    bigdecimal_destroy(value);
    bigdecimal_destroy(negative);
    bigdecimal_destroy(result);
}

void test_zero_identities_avoid_extreme_scale_work(void)
{
    BigDecimal *zero = bigdecimal_create();
    BigDecimal *tiny = make_decimal("1e-9223372036854775807");
    BigDecimal *negative_tiny = make_decimal("-1e-9223372036854775807");
    BigDecimal *result = make_decimal("42");
    int comparison = 1;

    TEST_ASSERT_NOT_NULL(zero);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_add(result, zero, tiny));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_compare(&comparison, result, tiny));
    TEST_ASSERT_EQUAL_INT(0, comparison);

    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_sub(result, zero, tiny));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_compare(&comparison, result, negative_tiny));
    TEST_ASSERT_EQUAL_INT(0, comparison);

    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_add(result, tiny, zero));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_compare(&comparison, result, tiny));
    TEST_ASSERT_EQUAL_INT(0, comparison);

    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,
                      bigdecimal_rescale(result, zero, INT64_MIN,
                                         BIGDECIMAL_ROUND_HALF_EVEN));
    assert_decimal_equals("0", result);

    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,
                      bigdecimal_div(result, zero, tiny, INT64_MAX,
                                     BIGDECIMAL_ROUND_HALF_EVEN));
    assert_decimal_equals("0", result);

    bigdecimal_destroy(zero);
    bigdecimal_destroy(tiny);
    bigdecimal_destroy(negative_tiny);
    bigdecimal_destroy(result);
}

/* ============================================================
   Rounded arithmetic
   ============================================================ */

void test_division_and_rounding(void)
{
    BigDecimal *one = make_decimal("1");
    BigDecimal *two = make_decimal("2");
    BigDecimal *three = make_decimal("3");
    BigDecimal *eight = make_decimal("8");
    BigDecimal *negative_one = make_decimal("-1");
    BigDecimal *zero = bigdecimal_create();
    BigDecimal *result = bigdecimal_create();

    TEST_ASSERT_NOT_NULL(zero);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_div(result, one, three, 4, BIGDECIMAL_ROUND_TOWARD_ZERO));
    assert_decimal_equals("0.3333", result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_div(result, one, two, 0, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_decimal_equals("0", result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_div(result, three, two, 0, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_decimal_equals("2", result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_div(result, one, eight, 2, BIGDECIMAL_ROUND_HALF_UP));
    assert_decimal_equals("0.13", result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_div(result, one, eight, 2, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_decimal_equals("0.12", result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_div(result, negative_one, two, 0, BIGDECIMAL_ROUND_FLOOR));
    assert_decimal_equals("-1", result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_div(three, three, two, 1, BIGDECIMAL_ROUND_TOWARD_ZERO));
    assert_decimal_equals("1.5", three);
    TEST_ASSERT_EQUAL(BIGDECIMAL_DIVISION_BY_ZERO, bigdecimal_div(result, one, zero, 0, BIGDECIMAL_ROUND_TOWARD_ZERO));

    bigdecimal_destroy(one);
    bigdecimal_destroy(two);
    bigdecimal_destroy(three);
    bigdecimal_destroy(eight);
    bigdecimal_destroy(negative_one);
    bigdecimal_destroy(zero);
    bigdecimal_destroy(result);
}

void test_division_cancels_extreme_scales_before_reporting_overflow(void)
{
    static const char *const inputs[] = {
        "1e-9223372036854775807", "10e9223372036854775807"
    };
    static const int64_t scales[] = { 1, -1 };
    static const char *const expected[] = { "1", "0" };

    for (size_t index = 0U; index < sizeof(inputs) / sizeof(inputs[0]); index++)
    {
        BigDecimal *value = make_decimal(inputs[index]);
        BigDecimalStatus status = bigdecimal_div(
            value, value, value, scales[index], BIGDECIMAL_ROUND_TOWARD_ZERO);

        /* Check status after cleanup on failure, including on the old code. */
        if (status != BIGDECIMAL_OK)
        {
            bigdecimal_destroy(value);
            TEST_ASSERT_EQUAL(BIGDECIMAL_OK, status);
        }
        assert_decimal_equals(expected[index], value);
        bigdecimal_destroy(value);
    }
}

void test_division_rejects_true_scale_overflow_without_modifying_result(void)
{
    static const char *const divisors[] = { "0.1", "10" };
    static const int64_t scales[] = { INT64_MAX, INT64_MIN };

    for (size_t index = 0U; index < sizeof(divisors) / sizeof(divisors[0]); index++)
    {
        BigDecimal *value = make_decimal("1");
        BigDecimal *divisor = make_decimal(divisors[index]);
        BigDecimal *result = make_decimal("42");
        BigDecimalStatus status = bigdecimal_div(
            result, value, divisor, scales[index], BIGDECIMAL_ROUND_TOWARD_ZERO);

        assert_decimal_equals("42", result);
        assert_decimal_equals("1", value);
        assert_decimal_equals(divisors[index], divisor);
        bigdecimal_destroy(result);
        bigdecimal_destroy(divisor);
        bigdecimal_destroy(value);
        TEST_ASSERT_EQUAL(BIGDECIMAL_SCALE_OVERFLOW, status);
    }
}

void test_multiplication_normalizes_before_extreme_scale_checks(void)
{
    static const struct { const char *a; const char *b; const char *expected; } cases[] = {
        { "2e-9223372036854775807", "0.5", "1e-9223372036854775807" },
        { "-2e-9223372036854775807", "0.5", "-1e-9223372036854775807" },
        { "2e9223372036854775807", "5", "10e9223372036854775807" },
        { "10e9223372036854775807", "0", "0" }
    };
    for (size_t i = 0U; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        for (size_t alias = 0U; alias < 3U; alias++)
        {
            BigDecimal *a = make_decimal(cases[i].a);
            BigDecimal *b = make_decimal(cases[i].b);
            BigDecimal *separate = make_decimal("42");
            BigDecimal *expected = make_decimal(cases[i].expected);
            BigDecimal *result = alias == 1U ? a : (alias == 2U ? b : separate);
            int comparison = 1;
            BigDecimalStatus status = bigdecimal_mul(result, a, b);
            BigDecimalStatus compared = bigdecimal_compare(&comparison, expected, result);
            bigdecimal_destroy(a);
            bigdecimal_destroy(b);
            bigdecimal_destroy(separate);
            bigdecimal_destroy(expected);
            TEST_ASSERT_EQUAL(BIGDECIMAL_OK, status);
            TEST_ASSERT_EQUAL(BIGDECIMAL_OK, compared);
            TEST_ASSERT_EQUAL_INT(0, comparison);
        }
    }
}

void test_multiplication_rejects_true_extreme_scale_overflow(void)
{
    static const char *const values[] = { "1e-9223372036854775807", "10e9223372036854775807" };
    static const char *const factors[] = { "0.1", "10" };
    for (size_t i = 0U; i < 2U; i++)
    {
        BigDecimal *a = make_decimal(values[i]);
        BigDecimal *b = make_decimal(factors[i]);
        BigDecimal *result = make_decimal("42");
        BigDecimalStatus status = bigdecimal_mul(result, a, b);
        assert_decimal_equals("42", result);
        bigdecimal_destroy(a);
        bigdecimal_destroy(b);
        bigdecimal_destroy(result);
        TEST_ASSERT_EQUAL(BIGDECIMAL_SCALE_OVERFLOW, status);
    }
}

void test_normalization_strips_decimal_zero_blocks(void)
{
    char input[160];
    static const size_t zero_counts[] = { 0U, 1U, 18U, 19U, 20U, 37U, 38U, 39U, 100U };
    for (size_t i = 0U; i < sizeof(zero_counts) / sizeof(zero_counts[0]); i++)
    {
        BigDecimal *value;
        BigDecimal *expected;
        char expected_text[64];
        int comparison = 1;
        size_t zeros = zero_counts[i];
        memcpy(input, "-12345", 6U);
        memset(input + 6U, '0', zeros);
        input[6U + zeros] = '\0';
        (void)snprintf(expected_text, sizeof(expected_text), "-12345e%zu", zeros);
        value = make_decimal(input);
        expected = make_decimal(expected_text);
        BigDecimalStatus status = bigdecimal_compare(&comparison, value, expected);
        bigdecimal_destroy(value);
        bigdecimal_destroy(expected);
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, status);
        TEST_ASSERT_EQUAL_INT(0, comparison);
    }
}

void test_null_and_invalid_arguments(void)
{
    BigDecimal *value = make_decimal("1");
    BigDecimal *other = make_decimal("2");
    BigDecimal *zero = bigdecimal_create();
    BigDecimal *result = make_decimal("42");
    char *string = NULL;
    bool predicate = false;
    int comparison = 0;

    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_set_string(NULL, "1"));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_set_string(value, NULL));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_to_string(value, NULL));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_to_string(NULL, &string));

    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_copy(NULL, value));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_copy(value, NULL));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_compare(NULL, value, other));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_compare(&comparison, NULL, other));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_compare(&comparison, value, NULL));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_is_zero(NULL, value));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_is_zero(&predicate, NULL));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_is_negative(NULL, value));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_is_negative(&predicate, NULL));

    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_abs(NULL, value));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_abs(result, NULL));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_negate(NULL, value));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_negate(result, NULL));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_add(NULL, value, other));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_add(result, NULL, other));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_add(result, value, NULL));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_sub(NULL, value, other));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_sub(result, NULL, other));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_sub(result, value, NULL));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_mul(NULL, value, other));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_mul(result, NULL, other));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_mul(result, value, NULL));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT,
                      bigdecimal_rescale(NULL, value, 0, BIGDECIMAL_ROUND_TOWARD_ZERO));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT,
                      bigdecimal_rescale(result, NULL, 0, BIGDECIMAL_ROUND_TOWARD_ZERO));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT,
                      bigdecimal_div(NULL, value, other, 0, BIGDECIMAL_ROUND_TOWARD_ZERO));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT,
                      bigdecimal_div(result, NULL, other, 0, BIGDECIMAL_ROUND_TOWARD_ZERO));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT,
                      bigdecimal_div(result, value, NULL, 0, BIGDECIMAL_ROUND_TOWARD_ZERO));

    TEST_ASSERT_EQUAL(BIGDECIMAL_INVALID_ARGUMENT,
                      bigdecimal_rescale(result, value, 0, (BigDecimalRoundingMode)99));
    assert_decimal_equals("42", result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_INVALID_ARGUMENT,
                      bigdecimal_div(result, value, other, 0, (BigDecimalRoundingMode)99));
    assert_decimal_equals("42", result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_DIVISION_BY_ZERO,
                      bigdecimal_div(result, value, zero, 0, BIGDECIMAL_ROUND_TOWARD_ZERO));
    assert_decimal_equals("42", result);

    bigdecimal_destroy(value);
    bigdecimal_destroy(other);
    bigdecimal_destroy(zero);
    bigdecimal_destroy(result);
}

/* ============================================================
   Main
   ============================================================ */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_lifetime_and_status_strings);
    RUN_TEST(test_parse_format_and_canonical_form);
    RUN_TEST(test_readable_format_thresholds_and_rounding_carry);
    RUN_TEST(test_invalid_parse_does_not_modify_destination);
    RUN_TEST(test_scale_overflow_does_not_modify_destination);
    RUN_TEST(test_copy_comparison_and_inspection);
    RUN_TEST(test_exact_arithmetic_and_aliasing);
    RUN_TEST(test_rescale_rounding);
    RUN_TEST(test_zero_identities_avoid_extreme_scale_work);
    RUN_TEST(test_division_and_rounding);
    RUN_TEST(test_division_cancels_extreme_scales_before_reporting_overflow);
    RUN_TEST(test_division_rejects_true_scale_overflow_without_modifying_result);
    RUN_TEST(test_multiplication_normalizes_before_extreme_scale_checks);
    RUN_TEST(test_multiplication_rejects_true_extreme_scale_overflow);
    RUN_TEST(test_normalization_strips_decimal_zero_blocks);
    RUN_TEST(test_null_and_invalid_arguments);

    return UNITY_END();
}
