#include <stdlib.h>

#include <unity.h>

#include <numforge/bigdecimal.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Public hyperbolic values, stable small and large arguments, inverse
    domains, rounding, aliasing, and destination preservation.
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

static void assert_text(const char *expected, const BigDecimal *value)
{
    char *text = NULL;

    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_to_string(value, &text));
    TEST_ASSERT_EQUAL_STRING(expected, text);
    free(text);
}

static void assert_unary(
    BigDecimalStatus (*operation)(
        BigDecimal *, const BigDecimal *, int64_t, BigDecimalRoundingMode),
    const char *input,
    int64_t digits,
    const char *expected
)
{
    BigDecimal *value = make_decimal(input);
    BigDecimal *result = bigdecimal_create();

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL(
        BIGDECIMAL_OK,
        operation(result, value, digits, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_text(expected, result);
    bigdecimal_destroy(value);
    bigdecimal_destroy(result);
}

static void test_known_values_and_identities(void)
{
    assert_unary(bigdecimal_sinh, "0", 20, "0");
    assert_unary(bigdecimal_cosh, "0", 20, "1");
    assert_unary(bigdecimal_tanh, "0", 20, "0");
    assert_unary(bigdecimal_asinh, "0", 20, "0");
    assert_unary(bigdecimal_acosh, "1", 20, "0");
    assert_unary(bigdecimal_atanh, "0", 20, "0");
    assert_unary(bigdecimal_sinh, "1", 20, "1.1752011936438014569");
    assert_unary(bigdecimal_cosh, "1", 20, "1.5430806348152437785");
    assert_unary(bigdecimal_tanh, "1", 20, "0.76159415595576488812");
    assert_unary(bigdecimal_asinh, "1", 20, "0.88137358701954302523");
    assert_unary(bigdecimal_acosh, "2", 20, "1.3169578969248167086");
    assert_unary(bigdecimal_atanh, "0.5", 20, "0.5493061443340548457");
}

static void test_parity_aliasing_and_extreme_arguments(void)
{
    BigDecimal *value = make_decimal("-1");

    TEST_ASSERT_EQUAL(
        BIGDECIMAL_OK,
        bigdecimal_sinh(value, value, 20, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_text("-1.1752011936438014569", value);

    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, "-1"));
    TEST_ASSERT_EQUAL(
        BIGDECIMAL_OK,
        bigdecimal_cosh(value, value, 20, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_text("1.5430806348152437785", value);

    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, "1E100"));
    TEST_ASSERT_EQUAL(
        BIGDECIMAL_OK,
        bigdecimal_tanh(value, value, 20, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_text("1", value);

    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, "-1E100"));
    TEST_ASSERT_EQUAL(
        BIGDECIMAL_OK,
        bigdecimal_tanh(value, value, 20, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_text("-1", value);

    bigdecimal_destroy(value);
}

static void test_small_arguments_preserve_information(void)
{
    typedef BigDecimalStatus (*UnaryOperation)(
        BigDecimal *, const BigDecimal *, int64_t, BigDecimalRoundingMode);
    static const UnaryOperation operations[] =
    {
        bigdecimal_sinh,
        bigdecimal_tanh,
        bigdecimal_asinh,
        bigdecimal_atanh
    };
    BigDecimal *input = make_decimal("1E-100");
    BigDecimal *result = bigdecimal_create();

    TEST_ASSERT_NOT_NULL(result);

    for (size_t index = 0; index < sizeof(operations) / sizeof(operations[0]); index++)
    {
        int comparison = 1;

        TEST_ASSERT_EQUAL(
            BIGDECIMAL_OK,
            operations[index](result, input, 20, BIGDECIMAL_ROUND_HALF_EVEN));
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_compare(&comparison, result, input));
        TEST_ASSERT_EQUAL_INT(0, comparison);
    }

    bigdecimal_destroy(input);
    bigdecimal_destroy(result);
}

static void test_inverse_functions_near_domains_and_at_large_magnitude(void)
{
    assert_unary(
        bigdecimal_asinh,
        "1E100",
        20,
        "230.95165647996451371");
    assert_unary(
        bigdecimal_acosh,
        "1.00000000000000000001",
        20,
        "0.00000000014142135623730950488");
    assert_unary(
        bigdecimal_atanh,
        "0.9999999999",
        20,
        "11.859499055225201075");
}

static void test_domains_rounding_and_invalid_arguments(void)
{
    BigDecimal *input = make_decimal("0.5");
    BigDecimal *result = make_decimal("7.77");
    BigDecimal *outside = make_decimal("0.999");

    TEST_ASSERT_EQUAL(
        BIGDECIMAL_OK,
        bigdecimal_sinh(result, input, 5, BIGDECIMAL_ROUND_FLOOR));
    assert_text("0.52109", result);
    TEST_ASSERT_EQUAL(
        BIGDECIMAL_OK,
        bigdecimal_sinh(result, input, 5, BIGDECIMAL_ROUND_CEILING));
    assert_text("0.5211", result);

    TEST_ASSERT_EQUAL(
        BIGDECIMAL_INVALID_ARGUMENT,
        bigdecimal_acosh(result, outside, 20, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_text("0.5211", result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(outside, "1"));
    TEST_ASSERT_EQUAL(
        BIGDECIMAL_INVALID_ARGUMENT,
        bigdecimal_atanh(result, outside, 20, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_text("0.5211", result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(outside, "-1.01"));
    TEST_ASSERT_EQUAL(
        BIGDECIMAL_INVALID_ARGUMENT,
        bigdecimal_atanh(result, outside, 20, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_text("0.5211", result);

    TEST_ASSERT_EQUAL(
        BIGDECIMAL_INVALID_ARGUMENT,
        bigdecimal_sinh(result, input, 0, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(
        BIGDECIMAL_INVALID_ARGUMENT,
        bigdecimal_cosh(result, input, 20, (BigDecimalRoundingMode)99));
    TEST_ASSERT_EQUAL(
        BIGDECIMAL_NULL_ARGUMENT,
        bigdecimal_asinh(NULL, input, 20, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(
        BIGDECIMAL_NULL_ARGUMENT,
        bigdecimal_tanh(result, NULL, 20, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_text("0.5211", result);

    bigdecimal_destroy(input);
    bigdecimal_destroy(result);
    bigdecimal_destroy(outside);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_known_values_and_identities);
    RUN_TEST(test_parity_aliasing_and_extreme_arguments);
    RUN_TEST(test_small_arguments_preserve_information);
    RUN_TEST(test_inverse_functions_near_domains_and_at_large_magnitude);
    RUN_TEST(test_domains_rounding_and_invalid_arguments);
    return UNITY_END();
}
