#include <stdlib.h>

#include <unity.h>

#include <numforge/bigdecimal.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Public exponential and logarithmic values, domains, rounding, aliasing,
    compact magnitudes, and strong failure guarantees.
------------------------------------------------------------------------------------------------------------------------------
*/

void setUp(void)
{
}

void tearDown(void)
{
}

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

static void assert_decimal_close(
    const char *expected_text,
    const char *tolerance_text,
    const BigDecimal *actual
)
{
    BigDecimal *expected = make_decimal(expected_text);
    BigDecimal *difference = bigdecimal_create();
    BigDecimal *tolerance = make_decimal(tolerance_text);
    int comparison = 1;

    TEST_ASSERT_NOT_NULL(difference);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_sub(difference, actual, expected));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_abs(difference, difference));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_compare(&comparison, difference, tolerance));
    TEST_ASSERT_LESS_OR_EQUAL_INT(0, comparison);

    bigdecimal_destroy(expected);
    bigdecimal_destroy(difference);
    bigdecimal_destroy(tolerance);
}

static void test_exponential_and_logarithmic_values(void)
{
    BigDecimal *value = make_decimal("0");
    BigDecimal *base = make_decimal("2");
    BigDecimal *result = make_decimal("42");

    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,
                      bigdecimal_exp(result, value, 24, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_decimal_equals("1", result);

    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, "1"));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,
                      bigdecimal_exp(result, value, 24, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_decimal_close("2.718281828459045235360287", "1E-23", result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,
                      bigdecimal_ln(result, value, 24, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_decimal_equals("0", result);

    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, "2"));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,
                      bigdecimal_ln(result, value, 24, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_decimal_close("0.693147180559945309417232", "1E-23", result);

    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, "1000"));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,
                      bigdecimal_log10(result, value, 24, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_decimal_close("3", "1E-22", result);

    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, "8"));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,
                      bigdecimal_log(result, value, base, 24, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_decimal_close("3", "1E-22", result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,
                      bigdecimal_exp(result, result, 24, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_decimal_close("20.08553692318766774092853", "1E-22", result);

    bigdecimal_destroy(value);
    bigdecimal_destroy(base);
    bigdecimal_destroy(result);
}

static void test_compact_magnitudes_and_rounding(void)
{
    BigDecimal *value = make_decimal("-100");
    BigDecimal *result = bigdecimal_create();

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,
                      bigdecimal_exp(result, value, 24, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_decimal_close("3.720075976020835962959696E-44", "1E-67", result);

    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, "1E1000"));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,
                      bigdecimal_log10(result, value, 24, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_decimal_close("1000", "1E-20", result);

    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, "1"));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,
                      bigdecimal_exp(result, value, 5, BIGDECIMAL_ROUND_FLOOR));
    assert_decimal_equals("2.7182", result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,
                      bigdecimal_exp(result, value, 5, BIGDECIMAL_ROUND_CEILING));
    assert_decimal_equals("2.7183", result);

    bigdecimal_destroy(value);
    bigdecimal_destroy(result);
}

static void test_inverse_identities(void)
{
    static const char *const positive_values[] = { "0.1", "0.5", "2", "10", "123.456" };
    static const char *const exponential_values[] = { "-10", "-1", "0.5", "5" };
    BigDecimal *value = bigdecimal_create();
    BigDecimal *result = bigdecimal_create();

    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_NOT_NULL(result);

    for (size_t index = 0U;
         index < sizeof(positive_values) / sizeof(positive_values[0]);
         index++)
    {
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, positive_values[index]));
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK,
                          bigdecimal_ln(result, value, 30, BIGDECIMAL_ROUND_HALF_EVEN));
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK,
                          bigdecimal_exp(result, result, 24, BIGDECIMAL_ROUND_HALF_EVEN));
        assert_decimal_close(positive_values[index], "1E-20", result);
    }

    for (size_t index = 0U;
         index < sizeof(exponential_values) / sizeof(exponential_values[0]);
         index++)
    {
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, exponential_values[index]));
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK,
                          bigdecimal_exp(result, value, 30, BIGDECIMAL_ROUND_HALF_EVEN));
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK,
                          bigdecimal_ln(result, result, 24, BIGDECIMAL_ROUND_HALF_EVEN));
        assert_decimal_close(exponential_values[index], "1E-20", result);
    }

    bigdecimal_destroy(value);
    bigdecimal_destroy(result);
}

static void test_domains_and_failure_safety(void)
{
    BigDecimal *value = make_decimal("0");
    BigDecimal *base = make_decimal("10");
    BigDecimal *result = make_decimal("42");

    TEST_ASSERT_EQUAL(BIGDECIMAL_INVALID_ARGUMENT,
                      bigdecimal_ln(result, value, 20, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_decimal_equals("42", result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, "-1"));
    TEST_ASSERT_EQUAL(BIGDECIMAL_INVALID_ARGUMENT,
                      bigdecimal_log10(result, value, 20, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_decimal_equals("42", result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, "8"));

    const char *invalid_bases[] = { "1", "-2", "0" };

    for (size_t index = 0U; index < sizeof(invalid_bases) / sizeof(invalid_bases[0]); index++)
    {
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(base, invalid_bases[index]));
        TEST_ASSERT_EQUAL(BIGDECIMAL_INVALID_ARGUMENT,
                          bigdecimal_log(result, value, base, 20, BIGDECIMAL_ROUND_HALF_EVEN));
        assert_decimal_equals("42", result);
    }

    TEST_ASSERT_EQUAL(BIGDECIMAL_INVALID_ARGUMENT,
                      bigdecimal_exp(result, value, 0, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_VALUE_TOO_LARGE,
                      bigdecimal_exp(result, value, INT64_MAX, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_INVALID_ARGUMENT,
                      bigdecimal_ln(result, value, 20, (BigDecimalRoundingMode)99));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT,
                      bigdecimal_exp(NULL, value, 20, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT,
                      bigdecimal_ln(result, NULL, 20, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT,
                      bigdecimal_log(result, value, NULL, 20, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_decimal_equals("42", result);

    bigdecimal_destroy(value);
    bigdecimal_destroy(base);
    bigdecimal_destroy(result);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_exponential_and_logarithmic_values);
    RUN_TEST(test_compact_magnitudes_and_rounding);
    RUN_TEST(test_inverse_identities);
    RUN_TEST(test_domains_and_failure_safety);
    return UNITY_END();
}
