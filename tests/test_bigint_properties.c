#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "unity.h"
#include "numforge/bigint.h"

/* A fixed seed makes every generated case reproducible in CI and locally. */
static uint64_t random_state = UINT64_C(0xC0DEC0FFEE123456);

static uint64_t next_random_u64(void)
{
    uint64_t value = random_state;

    value ^= value >> 12;
    value ^= value << 25;
    value ^= value >> 27;
    random_state = value;

    return value * UINT64_C(2685821657736338717);
}

static BigInt *make_bigint(const char *string)
{
    BigInt *value = bigint_create();

    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(value, string));

    return value;
}

static BigInt *make_random_unsigned(size_t limbs)
{
    char text[32];
    (void)snprintf(text, sizeof(text), "%" PRIu64, next_random_u64() | 1ULL);

    BigInt *value = make_bigint(text);

    for (size_t i = 1; i < limbs; i++)
    {
        BigInt *part;

        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_shift_left(value, value, 64));

        (void)snprintf(text, sizeof(text), "%" PRIu64, next_random_u64());
        part = make_bigint(text);
        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_add(value, value, part));
        bigint_destroy(part);
    }

    return value;
}

static BigInt *make_random_signed(void)
{
    BigInt *value = make_random_unsigned((size_t)(next_random_u64() % 4) + 1);

    if ((next_random_u64() & 1ULL) != 0)
    {
        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_negate(value, value));
    }

    return value;
}

static void assert_equal(const BigInt *actual, const BigInt *expected)
{
    TEST_ASSERT_EQUAL_INT(0, bigint_compare(actual, expected));
}

static void assert_string(const BigInt *value, const char *expected)
{
    char *actual = bigint_to_string(value);

    TEST_ASSERT_NOT_NULL(actual);
    TEST_ASSERT_EQUAL_STRING(expected, actual);
    free(actual);
}

void setUp(void)
{
    random_state = UINT64_C(0xC0DEC0FFEE123456);
}

void tearDown(void)
{
}

void test_multi_limb_boundaries(void)
{
    BigInt *max_128 = make_bigint("340282366920938463463374607431768211455");
    BigInt *two_64 = make_bigint("18446744073709551616");
    BigInt *one = make_bigint("1");
    BigInt *result = bigint_create();
    BigInt *quotient = bigint_create();
    BigInt *remainder = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_add(result, max_128, one));
    assert_string(result, "340282366920938463463374607431768211456");

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_div_mod(quotient, remainder, max_128, two_64));
    assert_string(quotient, "18446744073709551615");
    assert_string(remainder, "18446744073709551615");

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_or(result, two_64, one));
    assert_string(result, "18446744073709551617");

    bigint_destroy(max_128);
    bigint_destroy(two_64);
    bigint_destroy(one);
    bigint_destroy(result);
    bigint_destroy(quotient);
    bigint_destroy(remainder);
}

void test_div_mod_output_input_aliasing(void)
{
    BigInt *a = make_bigint("340282366920938463463374607431768211455");
    BigInt *b = make_bigint("18446744073709551616");

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_div_mod(a, b, a, b));
    assert_string(a, "18446744073709551615");
    assert_string(b, "18446744073709551615");

    bigint_destroy(a);
    bigint_destroy(b);
}

void test_property_string_round_trip_and_add_sub_inverse(void)
{
    for (size_t i = 0; i < 160; i++)
    {
        BigInt *a = make_random_signed();
        BigInt *b = make_random_signed();
        BigInt *parsed = bigint_create();
        BigInt *sum = bigint_create();
        char *text = bigint_to_string(a);

        TEST_ASSERT_NOT_NULL(text);
        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(parsed, text));
        assert_equal(parsed, a);

        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_add(sum, a, b));
        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_sub(sum, sum, b));
        assert_equal(sum, a);

        free(text);
        bigint_destroy(a);
        bigint_destroy(b);
        bigint_destroy(parsed);
        bigint_destroy(sum);
    }
}

void test_property_division_identity_and_remainder_bound(void)
{
    for (size_t i = 0; i < 96; i++)
    {
        BigInt *a = make_random_signed();
        BigInt *b = make_random_signed();
        BigInt *quotient = bigint_create();
        BigInt *remainder = bigint_create();
        BigInt *reconstructed = bigint_create();
        BigInt *abs_remainder = bigint_create();
        BigInt *abs_b = bigint_create();

        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_div_mod(quotient, remainder, a, b));
        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_mul(reconstructed, quotient, b));
        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_add(reconstructed, reconstructed, remainder));
        assert_equal(reconstructed, a);

        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_abs(abs_remainder, remainder));
        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_abs(abs_b, b));
        TEST_ASSERT_TRUE(bigint_compare(abs_remainder, abs_b) < 0);

        bigint_destroy(a);
        bigint_destroy(b);
        bigint_destroy(quotient);
        bigint_destroy(remainder);
        bigint_destroy(reconstructed);
        bigint_destroy(abs_remainder);
        bigint_destroy(abs_b);
    }
}

void test_property_product_division_and_shifts(void)
{
    for (size_t i = 0; i < 96; i++)
    {
        BigInt *a = make_random_unsigned((size_t)(next_random_u64() % 4) + 1);
        BigInt *b = make_random_unsigned((size_t)(next_random_u64() % 4) + 1);
        BigInt *product = bigint_create();
        BigInt *quotient = bigint_create();
        BigInt *remainder = bigint_create();
        BigInt *shifted = bigint_create();
        size_t shift = (size_t)(next_random_u64() % 192);

        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_mul(product, a, b));
        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_div_mod(quotient, remainder, product, a));
        assert_equal(quotient, b);
        TEST_ASSERT_TRUE(bigint_is_zero(remainder));

        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_shift_left(shifted, a, shift));
        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_shift_right(shifted, shifted, shift));
        assert_equal(shifted, a);

        bigint_destroy(a);
        bigint_destroy(b);
        bigint_destroy(product);
        bigint_destroy(quotient);
        bigint_destroy(remainder);
        bigint_destroy(shifted);
    }
}

void test_property_gcd_lcm_identity(void)
{
    for (size_t i = 0; i < 48; i++)
    {
        BigInt *a = make_random_unsigned((size_t)(next_random_u64() % 3) + 1);
        BigInt *b = make_random_unsigned((size_t)(next_random_u64() % 3) + 1);
        BigInt *gcd = bigint_create();
        BigInt *lcm = bigint_create();
        BigInt *left = bigint_create();
        BigInt *right = bigint_create();
        BigInt *remainder = bigint_create();

        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_gcd(gcd, a, b));
        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_mod(remainder, a, gcd));
        TEST_ASSERT_TRUE(bigint_is_zero(remainder));
        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_mod(remainder, b, gcd));
        TEST_ASSERT_TRUE(bigint_is_zero(remainder));

        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_lcm(lcm, a, b));
        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_mul(left, lcm, gcd));
        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_mul(right, a, b));
        assert_equal(left, right);

        bigint_destroy(a);
        bigint_destroy(b);
        bigint_destroy(gcd);
        bigint_destroy(lcm);
        bigint_destroy(left);
        bigint_destroy(right);
        bigint_destroy(remainder);
    }
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_multi_limb_boundaries);
    RUN_TEST(test_div_mod_output_input_aliasing);
    RUN_TEST(test_property_string_round_trip_and_add_sub_inverse);
    RUN_TEST(test_property_division_identity_and_remainder_bound);
    RUN_TEST(test_property_product_division_and_shifts);
    RUN_TEST(test_property_gcd_lcm_identity);

    return UNITY_END();
}
