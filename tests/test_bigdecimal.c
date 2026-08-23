#include <stdlib.h>

#include <unity.h>

#include <numforge/bigdecimal.h>

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

void test_null_and_invalid_arguments(void)
{
    BigDecimal *value = make_decimal("1");
    char *string = NULL;

    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_set_string(NULL, "1"));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_to_string(value, NULL));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_to_string(NULL, &string));
    TEST_ASSERT_EQUAL(BIGDECIMAL_INVALID_ARGUMENT,
                      bigdecimal_rescale(value, value, 0, (BigDecimalRoundingMode)99));

    bigdecimal_destroy(value);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_lifetime_and_status_strings);
    RUN_TEST(test_parse_format_and_canonical_form);
    RUN_TEST(test_invalid_parse_does_not_modify_destination);
    RUN_TEST(test_scale_overflow_does_not_modify_destination);
    RUN_TEST(test_copy_comparison_and_inspection);
    RUN_TEST(test_exact_arithmetic_and_aliasing);
    RUN_TEST(test_rescale_rounding);
    RUN_TEST(test_division_and_rounding);
    RUN_TEST(test_null_and_invalid_arguments);

    return UNITY_END();
}
