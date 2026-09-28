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

static BigInt *integer(const char *text)
{
    BigInt *value = bigint_create();
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(value, text));
    return value;
}

static BigRational *fraction(const char *numerator, const char *denominator)
{
    BigInt *top = integer(numerator);
    BigInt *bottom = integer(denominator);
    BigRational *value = bigrational_create();
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_set_fraction(value, top, bottom));
    bigint_destroy(top);
    bigint_destroy(bottom);
    return value;
}

static void assert_fraction(const char *expected, const BigRational *value)
{
    char *text = NULL;
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_to_string(value, &text));
    TEST_ASSERT_EQUAL_STRING(expected, text);
    free(text);
}

static void test_canonical_lifetime_and_parts(void)
{
    BigRational *value = bigrational_create();
    BigInt *top = integer("99"), *bottom = integer("99");
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_STRING("success", bigrational_status_to_string(BIGRATIONAL_OK));
    TEST_ASSERT_EQUAL_STRING("unknown status", bigrational_status_to_string((BigRationalStatus)999));
    assert_fraction("0", value);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_set_fraction(value, top, bottom));
    assert_fraction("1", value);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_get_numerator(top, value));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_get_denominator(bottom, value));
    TEST_ASSERT_TRUE(bigint_is_one(top));
    TEST_ASSERT_TRUE(bigint_is_one(bottom));
    bigint_destroy(top);
    bigint_destroy(bottom);
    bigrational_destroy(value);
    bigrational_destroy(NULL);
    value = fraction("-42", "-56");
    assert_fraction("3/4", value);
    bigrational_destroy(value);
    value = fraction("0", "-999");
    assert_fraction("0", value);
    bigrational_destroy(value);
    value = fraction("42", "-56");
    assert_fraction("-3/4", value);
    bigrational_destroy(value);
}

static void test_arithmetic_comparison_and_aliases(void)
{
    BigRational *a = fraction("1", "6"), *b = fraction("1", "3"), *out = bigrational_create();
    int comparison = 42;
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_add(out, a, b));
    assert_fraction("1/2", out);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_sub(out, a, b));
    assert_fraction("-1/6", out);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_mul(out, a, b));
    assert_fraction("1/18", out);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_div(out, a, b));
    assert_fraction("1/2", out);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_compare(&comparison, a, b));
    TEST_ASSERT_TRUE(comparison < 0);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_add(a, a, b));
    assert_fraction("1/2", a);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_mul(a, a, a));
    assert_fraction("1/4", a);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_negate(a, a));
    assert_fraction("-1/4", a);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_abs(a, a));
    assert_fraction("1/4", a);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_copy(a, a));
    assert_fraction("1/4", a);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_copy(out, a));
    assert_fraction("1/4", out);
    bigrational_destroy(a);
    bigrational_destroy(b);
    bigrational_destroy(out);

    a = fraction("12345678901234567890", "99999999999999999999");
    b = fraction("99999999999999999999", "12345678901234567890");
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_mul(a, a, b));
    assert_fraction("1", a);
    bigrational_destroy(a);
    bigrational_destroy(b);
}

static void test_decimal_conversion_and_rounding(void)
{
    BigDecimal *decimal = bigdecimal_create();
    BigRational *value = bigrational_create();
    BigInt *whole = integer("123456789012345678901234567890");
    char *text = NULL;
    TEST_ASSERT_NOT_NULL(decimal);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(decimal, "-12.500"));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_from_bigdecimal(value, decimal));
    assert_fraction("-25/2", value);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_to_bigdecimal(decimal, value, 1, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_to_string(decimal, &text));
    TEST_ASSERT_EQUAL_STRING("-12.5", text);
    free(text);
    text = NULL;
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_from_bigint(value, whole));
    assert_fraction("123456789012345678901234567890", value);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(decimal, "1E+20"));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_from_bigdecimal(value, decimal));
    assert_fraction("100000000000000000000", value);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(decimal, "1E-20"));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_from_bigdecimal(value, decimal));
    assert_fraction("1/100000000000000000000", value);
    bigint_destroy(whole);
    bigrational_destroy(value);
    bigdecimal_destroy(decimal);

    value = fraction("1", "3");
    decimal = bigdecimal_create();
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_to_bigdecimal(decimal, value, 5, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_to_string(decimal, &text));
    TEST_ASSERT_EQUAL_STRING("0.33333", text);
    free(text);
    bigrational_destroy(value);
    bigdecimal_destroy(decimal);

    value = fraction("-1", "6");
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
    BigRational *value = fraction("7", "8"), *zero = bigrational_create();
    BigInt *top = integer("3"), *bottom = integer("0");
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
    assert_fraction("7/8", value);
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
    assert_fraction(expected, value);
}

static void test_small_integer_reference_grid(void)
{
    for (long long numerator_a = -4; numerator_a <= 4; numerator_a++)
    {
        for (long long numerator_b = -4; numerator_b <= 4; numerator_b++)
        {
            BigRational *a = fraction("1", "2");
            BigRational *b = fraction("1", "3");
            BigRational *out = bigrational_create();
            BigInt *top_a = bigint_create(), *top_b = bigint_create();
            BigInt *denom_a = integer("2"), *denom_b = integer("3");
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
