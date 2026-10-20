#include "tests/support/numbers.h"
#include <numforge/bigrational.h>

#include <unity.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void setUp(void)
{
}
void tearDown(void)
{
}

static void test_canonical_lifetime_and_parts(void)
{
    BigRational *value = bigrational_create();
    BigInt *top = test_integer("99"), *bottom = test_integer("99");
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_STRING("success", bigrational_status_to_string(BIGRATIONAL_OK));
    TEST_ASSERT_EQUAL_STRING("unknown status", bigrational_status_to_string((BigRationalStatus)999));
    test_assert_fraction("0", value);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_set_fraction(value, top, bottom));
    test_assert_fraction("1", value);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_get_numerator(top, value));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_get_denominator(bottom, value));
    TEST_ASSERT_TRUE(bigint_is_one(top));
    TEST_ASSERT_TRUE(bigint_is_one(bottom));
    bigint_destroy(top);
    bigint_destroy(bottom);
    bigrational_destroy(value);
    bigrational_destroy(NULL);
    value = test_fraction("-42", "-56");
    test_assert_fraction("3/4", value);
    bigrational_destroy(value);
    value = test_fraction("0", "-999");
    test_assert_fraction("0", value);
    bigrational_destroy(value);
    value = test_fraction("42", "-56");
    test_assert_fraction("-3/4", value);
    bigrational_destroy(value);
}

static void test_arithmetic_comparison_and_aliases(void)
{
    BigRational *a = test_fraction("1", "6"), *b = test_fraction("1", "3"), *out = bigrational_create();
    int comparison = 42;
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_add(out, a, b));
    test_assert_fraction("1/2", out);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_sub(out, a, b));
    test_assert_fraction("-1/6", out);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_mul(out, a, b));
    test_assert_fraction("1/18", out);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_div(out, a, b));
    test_assert_fraction("1/2", out);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_compare(&comparison, a, b));
    TEST_ASSERT_TRUE(comparison < 0);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_add(a, a, b));
    test_assert_fraction("1/2", a);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_mul(a, a, a));
    test_assert_fraction("1/4", a);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_negate(a, a));
    test_assert_fraction("-1/4", a);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_abs(a, a));
    test_assert_fraction("1/4", a);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_copy(a, a));
    test_assert_fraction("1/4", a);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_copy(out, a));
    test_assert_fraction("1/4", out);
    bigrational_destroy(a);
    bigrational_destroy(b);
    bigrational_destroy(out);

    a = test_fraction("12345678901234567890", "99999999999999999999");
    b = test_fraction("99999999999999999999", "12345678901234567890");
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_mul(a, a, b));
    test_assert_fraction("1", a);
    bigrational_destroy(a);
    bigrational_destroy(b);
}

static void test_decimal_conversion_and_rounding(void)
{
    BigDecimal *decimal = bigdecimal_create();
    BigRational *value = bigrational_create();
    BigInt *whole = test_integer("123456789012345678901234567890");
    char *text = NULL;
    TEST_ASSERT_NOT_NULL(decimal);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(decimal, "-12.500"));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_from_bigdecimal(value, decimal));
    test_assert_fraction("-25/2", value);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_to_bigdecimal(decimal, value, 1, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_to_string(decimal, &text));
    TEST_ASSERT_EQUAL_STRING("-12.5", text);
    free(text);
    text = NULL;
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_from_bigint(value, whole));
    test_assert_fraction("123456789012345678901234567890", value);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(decimal, "1E+20"));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_from_bigdecimal(value, decimal));
    test_assert_fraction("100000000000000000000", value);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(decimal, "1E-20"));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_from_bigdecimal(value, decimal));
    test_assert_fraction("1/100000000000000000000", value);
    bigint_destroy(whole);
    bigrational_destroy(value);
    bigdecimal_destroy(decimal);

    value = test_fraction("1", "3");
    decimal = bigdecimal_create();
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_to_bigdecimal(decimal, value, 5, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_to_string(decimal, &text));
    TEST_ASSERT_EQUAL_STRING("0.33333", text);
    free(text);
    bigrational_destroy(value);
    bigdecimal_destroy(decimal);

    value = test_fraction("-1", "6");
    decimal = bigdecimal_create();
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK,
                      bigrational_to_bigdecimal(decimal, value, 1, BIGDECIMAL_ROUND_TOWARD_ZERO));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_to_string(decimal, &text));
    TEST_ASSERT_EQUAL_STRING("-0.1", text);
    free(text);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK,
                      bigrational_to_bigdecimal(decimal, value, 1, BIGDECIMAL_ROUND_FLOOR));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_to_string(decimal, &text));
    TEST_ASSERT_EQUAL_STRING("-0.2", text);
    free(text);
    bigrational_destroy(value);
    bigdecimal_destroy(decimal);
}

static void test_domains_preserve_outputs(void)
{
    BigRational *value = test_fraction("7", "8"), *zero = bigrational_create();
    BigInt *top = test_integer("3"), *bottom = test_integer("0");
    BigDecimal *decimal = bigdecimal_create();
    char *text = (char *)"sentinel";
    char *original_text = text;
    int comparison = 42;
    TEST_ASSERT_NOT_NULL(zero);
    TEST_ASSERT_NOT_NULL(decimal);
    TEST_ASSERT_EQUAL(BIGRATIONAL_DIVISION_BY_ZERO, bigrational_set_fraction(value, top, bottom));
    TEST_ASSERT_EQUAL(BIGRATIONAL_DIVISION_BY_ZERO, bigrational_div(value, value, zero));
    TEST_ASSERT_EQUAL(BIGRATIONAL_NULL_ARGUMENT, bigrational_compare(&comparison, value, NULL));
    TEST_ASSERT_EQUAL_INT(42, comparison);
    TEST_ASSERT_EQUAL(BIGRATIONAL_INVALID_ARGUMENT,
                      bigrational_to_bigdecimal(decimal, value, 0, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGRATIONAL_INVALID_ARGUMENT,
                      bigrational_to_bigdecimal(decimal, value, 10, (BigDecimalRoundingMode)999));
    TEST_ASSERT_EQUAL(BIGRATIONAL_NULL_ARGUMENT, bigrational_to_string(NULL, &text));
    TEST_ASSERT_EQUAL_PTR(original_text, text);
    test_assert_fraction("7/8", value);
    bigint_destroy(top);
    bigint_destroy(bottom);
    bigrational_destroy(value);
    bigrational_destroy(zero);
    bigdecimal_destroy(decimal);
}

static long long reference_gcd(long long a, long long b)
{
    a = llabs(a);
    b = llabs(b);
    while (b != 0)
    {
        long long remainder = a % b;
        a = b;
        b = remainder;
    }
    return a;
}

static void assert_reference_fraction(long long numerator, long long denominator, const BigRational *value)
{
    char expected[96];
    long long divisor = reference_gcd(numerator, denominator);
    if (denominator < 0)
    {
        numerator = -numerator;
        denominator = -denominator;
    }
    if (numerator == 0)
    {
        (void)snprintf(expected, sizeof(expected), "0");
    }
    else if (denominator / divisor == 1)
    {
        (void)snprintf(expected, sizeof(expected), "%lld", numerator / divisor);
    }
    else
    {
        (void)snprintf(expected, sizeof(expected), "%lld/%lld", numerator / divisor, denominator / divisor);
    }
    test_assert_fraction(expected, value);
}

static void test_small_integer_reference_grid(void)
{
    for (long long numerator_a = -4; numerator_a <= 4; numerator_a++)
    {
        for (long long numerator_b = -4; numerator_b <= 4; numerator_b++)
        {
            BigRational *a = test_fraction("1", "2");
            BigRational *b = test_fraction("1", "3");
            BigRational *out = bigrational_create();
            BigInt *top_a = bigint_create(), *top_b = bigint_create();
            BigInt *denom_a = test_integer("2"), *denom_b = test_integer("3");
            char text[32];
            int comparison = 0;
            TEST_ASSERT_NOT_NULL(out);
            TEST_ASSERT_NOT_NULL(top_a);
            TEST_ASSERT_NOT_NULL(top_b);
            (void)snprintf(text, sizeof(text), "%lld", numerator_a);
            TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(top_a, text));
            (void)snprintf(text, sizeof(text), "%lld", numerator_b);
            TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(top_b, text));
            TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_set_fraction(a, top_a, denom_a));
            TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_set_fraction(b, top_b, denom_b));
            TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_add(out, a, b));
            assert_reference_fraction(3 * numerator_a + 2 * numerator_b, 6, out);
            TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_sub(out, a, b));
            assert_reference_fraction(3 * numerator_a - 2 * numerator_b, 6, out);
            TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_mul(out, a, b));
            assert_reference_fraction(numerator_a * numerator_b, 6, out);
            if (numerator_b != 0)
            {
                TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_div(out, a, b));
                assert_reference_fraction(3 * numerator_a, 2 * numerator_b, out);
            }
            TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_compare(&comparison, a, b));
            TEST_ASSERT_EQUAL_INT((3 * numerator_a > 2 * numerator_b) - (3 * numerator_a < 2 * numerator_b),
                                  comparison);
            bigint_destroy(top_a);
            bigint_destroy(top_b);
            bigint_destroy(denom_a);
            bigint_destroy(denom_b);
            bigrational_destroy(a);
            bigrational_destroy(b);
            bigrational_destroy(out);
        }
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_canonical_lifetime_and_parts);
    RUN_TEST(test_arithmetic_comparison_and_aliases);
    RUN_TEST(test_decimal_conversion_and_rounding);
    RUN_TEST(test_domains_preserve_outputs);
    RUN_TEST(test_small_integer_reference_grid);
    return UNITY_END();
}
