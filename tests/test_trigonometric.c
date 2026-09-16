#include <stdlib.h>

#include <unity.h>

#include <numforge/bigdecimal.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Public radian trigonometric values, argument reduction, inverse domains,
    rounding, aliasing, and destination preservation.
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

static void test_forward_known_values(void)
{
    assert_unary(bigdecimal_sin, "0", 20, "0");
    assert_unary(bigdecimal_cos, "0", 20, "1");
    assert_unary(bigdecimal_tan, "0", 20, "0");
    assert_unary(bigdecimal_sin, "0.5", 20, "0.47942553860420300027");
    assert_unary(bigdecimal_cos, "0.5", 20, "0.87758256189037271612");
    assert_unary(bigdecimal_tan, "0.5", 20, "0.54630248984379051326");
}

static void test_pi_argument_reduction_and_aliasing(void)
{
    BigDecimal *pi = bigdecimal_create();
    BigDecimal *six = make_decimal("6");
    BigDecimal *angle = bigdecimal_create();

    TEST_ASSERT_NOT_NULL(pi);
    TEST_ASSERT_NOT_NULL(angle);
    TEST_ASSERT_EQUAL(
        BIGDECIMAL_OK,
        bigdecimal_set_constant_significant(
            pi, BIGDECIMAL_CONSTANT_PI, 80, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(
        BIGDECIMAL_OK,
        bigdecimal_div_exact_or_significant(
            angle, pi, six, 80, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(
        BIGDECIMAL_OK,
        bigdecimal_sin(angle, angle, 30, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_text("0.5", angle);

    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_copy(angle, pi));
    TEST_ASSERT_EQUAL(
        BIGDECIMAL_OK,
        bigdecimal_cos(angle, angle, 30, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_text("-1", angle);

    bigdecimal_destroy(pi);
    bigdecimal_destroy(six);
    bigdecimal_destroy(angle);
}

static void test_large_argument_uses_extended_pi_reduction(void)
{
    BigDecimal *pi = bigdecimal_create();
    BigDecimal *six = make_decimal("6");
    BigDecimal *multiple = make_decimal("1E50");
    BigDecimal *sixth = bigdecimal_create();
    BigDecimal *angle = bigdecimal_create();

    TEST_ASSERT_NOT_NULL(pi);
    TEST_ASSERT_NOT_NULL(sixth);
    TEST_ASSERT_NOT_NULL(angle);
    TEST_ASSERT_EQUAL(
        BIGDECIMAL_OK,
        bigdecimal_set_constant_significant(
            pi, BIGDECIMAL_CONSTANT_PI, 140, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_mul(angle, pi, multiple));
    TEST_ASSERT_EQUAL(
        BIGDECIMAL_OK,
        bigdecimal_div_exact_or_significant(
            sixth, pi, six, 140, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_add(angle, angle, sixth));
    TEST_ASSERT_EQUAL(
        BIGDECIMAL_OK,
        bigdecimal_sin(angle, angle, 25, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_text("0.5", angle);

    bigdecimal_destroy(pi);
    bigdecimal_destroy(six);
    bigdecimal_destroy(multiple);
    bigdecimal_destroy(sixth);
    bigdecimal_destroy(angle);
}

static void test_inverse_known_values_and_domains(void)
{
    assert_unary(bigdecimal_atan, "1", 20, "0.78539816339744830962");
    assert_unary(bigdecimal_asin, "0.5", 20, "0.52359877559829887308");
    assert_unary(bigdecimal_acos, "0.5", 20, "1.0471975511965977462");
    assert_unary(bigdecimal_asin, "-1", 20, "-1.5707963267948966192");
    assert_unary(bigdecimal_acos, "-1", 20, "3.1415926535897932385");
    assert_unary(bigdecimal_acos,
        "0.9999999999999999999999999999999999999999999999999999999999999999",
        10, "0.00000000000000000000000000000001414213562");

    BigDecimal *value = make_decimal("7.77");
    BigDecimal *outside = make_decimal("1.0001");

    TEST_ASSERT_EQUAL(
        BIGDECIMAL_INVALID_ARGUMENT,
        bigdecimal_asin(value, outside, 20, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_text("7.77", value);
    TEST_ASSERT_EQUAL(
        BIGDECIMAL_INVALID_ARGUMENT,
        bigdecimal_acos(value, outside, 20, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_text("7.77", value);
    bigdecimal_destroy(value);
    bigdecimal_destroy(outside);
}

static void test_rounding_and_invalid_arguments(void)
{
    BigDecimal *input = make_decimal("0.5");
    BigDecimal *result = make_decimal("7.77");

    TEST_ASSERT_EQUAL(
        BIGDECIMAL_OK,
        bigdecimal_sin(result, input, 5, BIGDECIMAL_ROUND_FLOOR));
    assert_text("0.47942", result);
    TEST_ASSERT_EQUAL(
        BIGDECIMAL_OK,
        bigdecimal_sin(result, input, 5, BIGDECIMAL_ROUND_CEILING));
    assert_text("0.47943", result);
    TEST_ASSERT_EQUAL(
        BIGDECIMAL_INVALID_ARGUMENT,
        bigdecimal_sin(result, input, 0, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_text("0.47943", result);
    TEST_ASSERT_EQUAL(
        BIGDECIMAL_INVALID_ARGUMENT,
        bigdecimal_cos(result, input, 20, (BigDecimalRoundingMode)99));
    assert_text("0.47943", result);
    TEST_ASSERT_EQUAL(
        BIGDECIMAL_NULL_ARGUMENT,
        bigdecimal_atan(NULL, input, 20, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(
        BIGDECIMAL_NULL_ARGUMENT,
        bigdecimal_tan(result, NULL, 20, BIGDECIMAL_ROUND_HALF_EVEN));

    bigdecimal_destroy(input);
    bigdecimal_destroy(result);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_forward_known_values);
    RUN_TEST(test_pi_argument_reduction_and_aliasing);
    RUN_TEST(test_large_argument_uses_extended_pi_reduction);
    RUN_TEST(test_inverse_known_values_and_domains);
    RUN_TEST(test_rounding_and_invalid_arguments);
    return UNITY_END();
}
