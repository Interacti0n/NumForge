#include "tests/support/numbers.h"
#include <stdlib.h>
#include <string.h>

#include <unity.h>
#include <numforge/bigint.h>
#include "tests/fixtures/bigint_base_references.h"

/* Test helpers */

static void assert_status(BigIntStatus actual, BigIntStatus expected)
{
    TEST_ASSERT_EQUAL(expected, actual);
}

static void assert_bigint_predicate(
    BigIntStatus (*predicate)(bool *, const BigInt *),
    bool expected,
    const BigInt *value
)
{
    bool actual = !expected;

    TEST_ASSERT_EQUAL(BIGINT_OK, predicate(&actual, value));
    TEST_ASSERT_EQUAL(expected, actual);
}

/* Unity setup / teardown */

void setUp(void)
{
}

void tearDown(void)
{
}

/* Status */

void test_status_to_string(void)
{
    TEST_ASSERT_NOT_NULL(bigint_status_to_string(BIGINT_OK));
    TEST_ASSERT_NOT_NULL(bigint_status_to_string(BIGINT_NULL_ARGUMENT));
    TEST_ASSERT_NOT_NULL(bigint_status_to_string(BIGINT_OUT_OF_MEMORY));
    TEST_ASSERT_NOT_NULL(bigint_status_to_string(BIGINT_DIVISION_BY_ZERO));
    TEST_ASSERT_NOT_NULL(bigint_status_to_string(BIGINT_INVALID_ARGUMENT));
    TEST_ASSERT_NOT_NULL(bigint_status_to_string(BIGINT_NEGATIVE_ARGUMENT));
    TEST_ASSERT_NOT_NULL(bigint_status_to_string(BIGINT_VALUE_TOO_LARGE));
}

/* Create / destroy */

void test_create_destroy(void)
{
    BigInt *x = bigint_create();

    TEST_ASSERT_NOT_NULL(x);

    bigint_destroy(x);
}

void test_destroy_null(void)
{
    bigint_destroy(NULL);
}

/* set_string / to_string */

void test_set_string_zero(void)
{
    BigInt *x = test_integer("0");

    test_assert_integer(x, "0");

    bigint_destroy(x);
}

void test_set_string_positive(void)
{
    BigInt *x = test_integer("123456789");

    test_assert_integer(x, "123456789");

    bigint_destroy(x);
}

void test_set_string_negative(void)
{
    BigInt *x = test_integer("-123456789");

    test_assert_integer(x, "-123456789");

    bigint_destroy(x);
}

void test_set_string_plus_sign(void)
{
    BigInt *x = test_integer("+123456789");

    test_assert_integer(x, "123456789");

    bigint_destroy(x);
}

void test_set_string_large_number(void)
{
    BigInt *x = test_integer(
        "12345678901234567890123456789012345678901234567890"
    );

    test_assert_integer(
        x,
        "12345678901234567890123456789012345678901234567890"
    );

    bigint_destroy(x);
}

void test_set_string_leading_zeros(void)
{
    BigInt *x = test_integer("0000000000012345");

    test_assert_integer(x, "12345");

    bigint_destroy(x);
}

void test_set_string_negative_zero(void)
{
    BigInt *x = test_integer("-0");

    test_assert_integer(x, "0");
    TEST_ASSERT_FALSE(bigint_is_negative(x));

    bigint_destroy(x);
}

void test_set_string_empty(void)
{
    BigInt *x = bigint_create();

    TEST_ASSERT_NOT_NULL(x);

    assert_status(
        bigint_set_string(x, ""),
        BIGINT_INVALID_ARGUMENT
    );

    bigint_destroy(x);
}

void test_set_string_sign_only(void)
{
    BigInt *x = bigint_create();

    TEST_ASSERT_NOT_NULL(x);

    TEST_ASSERT_EQUAL(
        BIGINT_INVALID_ARGUMENT,
        bigint_set_string(x, "+")
    );

    TEST_ASSERT_EQUAL(
        BIGINT_INVALID_ARGUMENT,
        bigint_set_string(x, "-")
    );

    bigint_destroy(x);
}

void test_set_string_invalid_characters(void)
{
    BigInt *x = bigint_create();

    TEST_ASSERT_NOT_NULL(x);

    TEST_ASSERT_EQUAL(
        BIGINT_INVALID_ARGUMENT,
        bigint_set_string(x, "123abc")
    );

    TEST_ASSERT_EQUAL(
        BIGINT_INVALID_ARGUMENT,
        bigint_set_string(x, "abc123")
    );

    TEST_ASSERT_EQUAL(
        BIGINT_INVALID_ARGUMENT,
        bigint_set_string(x, "12.34")
    );

    bigint_destroy(x);
}

void test_set_string_null_arguments(void)
{
    BigInt *x = bigint_create();

    TEST_ASSERT_NOT_NULL(x);

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_set_string(NULL, "123")
    );

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_set_string(x, NULL)
    );

    bigint_destroy(x);
}

void test_to_string_null(void)
{
    char *result = bigint_to_string(NULL);

    TEST_ASSERT_NULL(result);
}

/* Copy */

void test_copy(void)
{
    BigInt *source = test_integer("-12345678901234567890");
    BigInt *destination = bigint_create();

    TEST_ASSERT_NOT_NULL(destination);

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_copy(destination, source)
    );

    test_assert_integer(destination, "-12345678901234567890");

    bigint_destroy(source);
    bigint_destroy(destination);
}

void test_copy_self(void)
{
    BigInt *x = test_integer("123456789");

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_copy(x, x)
    );

    test_assert_integer(x, "123456789");

    bigint_destroy(x);
}

void test_copy_null_arguments(void)
{
    BigInt *x = test_integer("123");

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_copy(NULL, x)
    );

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_copy(x, NULL)
    );

    bigint_destroy(x);
}

/* Comparison */

void test_compare_equal(void)
{
    BigInt *a = test_integer("12345");
    BigInt *b = test_integer("12345");

    TEST_ASSERT_EQUAL(0, bigint_compare(a, b));

    bigint_destroy(a);
    bigint_destroy(b);
}

void test_compare_less(void)
{
    BigInt *a = test_integer("12344");
    BigInt *b = test_integer("12345");

    TEST_ASSERT_TRUE(bigint_compare(a, b) < 0);

    bigint_destroy(a);
    bigint_destroy(b);
}

void test_compare_greater(void)
{
    BigInt *a = test_integer("12346");
    BigInt *b = test_integer("12345");

    TEST_ASSERT_TRUE(bigint_compare(a, b) > 0);

    bigint_destroy(a);
    bigint_destroy(b);
}

void test_compare_negative_numbers(void)
{
    BigInt *a = test_integer("-100");
    BigInt *b = test_integer("-50");

    TEST_ASSERT_TRUE(bigint_compare(a, b) < 0);
    TEST_ASSERT_TRUE(bigint_compare(b, a) > 0);

    bigint_destroy(a);
    bigint_destroy(b);
}

void test_compare_negative_positive(void)
{
    BigInt *a = test_integer("-1");
    BigInt *b = test_integer("1");

    TEST_ASSERT_TRUE(bigint_compare(a, b) < 0);
    TEST_ASSERT_TRUE(bigint_compare(b, a) > 0);

    bigint_destroy(a);
    bigint_destroy(b);
}

/* is_zero / is_one / is_negative */

void test_is_zero(void)
{
    BigInt *zero = test_integer("0");
    BigInt *one = test_integer("1");

    TEST_ASSERT_TRUE(bigint_is_zero(zero));
    TEST_ASSERT_FALSE(bigint_is_zero(one));

    bigint_destroy(zero);
    bigint_destroy(one);
}

void test_is_one(void)
{
    BigInt *one = test_integer("1");
    BigInt *two = test_integer("2");

    TEST_ASSERT_TRUE(bigint_is_one(one));
    TEST_ASSERT_FALSE(bigint_is_one(two));

    bigint_destroy(one);
    bigint_destroy(two);
}

void test_is_negative(void)
{
    BigInt *negative = test_integer("-123");
    BigInt *positive = test_integer("123");
    BigInt *zero = test_integer("0");

    TEST_ASSERT_TRUE(bigint_is_negative(negative));
    TEST_ASSERT_FALSE(bigint_is_negative(positive));
    TEST_ASSERT_FALSE(bigint_is_negative(zero));

    bigint_destroy(negative);
    bigint_destroy(positive);
    bigint_destroy(zero);
}

/* ABS */

void test_abs_positive(void)
{
    BigInt *a = test_integer("123");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_abs(result, a)
    );

    test_assert_integer(result, "123");

    bigint_destroy(a);
    bigint_destroy(result);
}

void test_abs_negative(void)
{
    BigInt *a = test_integer("-123");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_abs(result, a)
    );

    test_assert_integer(result, "123");

    bigint_destroy(a);
    bigint_destroy(result);
}

void test_abs_zero(void)
{
    BigInt *a = test_integer("0");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_abs(result, a)
    );

    test_assert_integer(result, "0");

    bigint_destroy(a);
    bigint_destroy(result);
}

/* NEGATE */

void test_negate_positive(void)
{
    BigInt *a = test_integer("123");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_negate(result, a)
    );

    test_assert_integer(result, "-123");

    bigint_destroy(a);
    bigint_destroy(result);
}

void test_negate_negative(void)
{
    BigInt *a = test_integer("-123");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_negate(result, a)
    );

    test_assert_integer(result, "123");

    bigint_destroy(a);
    bigint_destroy(result);
}

void test_negate_zero(void)
{
    BigInt *a = test_integer("0");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_negate(result, a)
    );

    test_assert_integer(result, "0");

    bigint_destroy(a);
    bigint_destroy(result);
}

/* ADD */

void test_add_positive(void)
{
    BigInt *a = test_integer("15");
    BigInt *b = test_integer("11");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_add(result, a, b));

    test_assert_integer(result, "26");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}

void test_add_negative(void)
{
    BigInt *a = test_integer("-15");
    BigInt *b = test_integer("-11");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_add(result, a, b));

    test_assert_integer(result, "-26");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}

void test_add_mixed_signs(void)
{
    BigInt *a = test_integer("-15");
    BigInt *b = test_integer("11");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_add(result, a, b));

    test_assert_integer(result, "-4");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}

void test_add_zero(void)
{
    BigInt *a = test_integer("123456789");
    BigInt *b = test_integer("0");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_add(result, a, b));

    test_assert_integer(result, "123456789");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}

void test_add_alias_left(void)
{
    BigInt *a = test_integer("15");
    BigInt *b = test_integer("11");

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_add(a, a, b));

    test_assert_integer(a, "26");

    bigint_destroy(a);
    bigint_destroy(b);
}

void test_add_alias_right(void)
{
    BigInt *a = test_integer("15");
    BigInt *b = test_integer("11");

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_add(b, a, b));

    test_assert_integer(b, "26");

    bigint_destroy(a);
    bigint_destroy(b);
}

void test_add_alias_all(void)
{
    BigInt *a = test_integer("15");

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_add(a, a, a));

    test_assert_integer(a, "30");

    bigint_destroy(a);
}

void test_add_across_limb_boundary(void)
{
    BigInt *a = test_integer("18446744073709551615");
    BigInt *b = test_integer("1");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_add(result, a, b));
    test_assert_integer(result, "18446744073709551616");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}

/* SUB */

void test_sub_positive(void)
{
    BigInt *a = test_integer("15");
    BigInt *b = test_integer("11");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_sub(result, a, b));

    test_assert_integer(result, "4");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}

void test_sub_negative_result(void)
{
    BigInt *a = test_integer("11");
    BigInt *b = test_integer("15");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_sub(result, a, b));

    test_assert_integer(result, "-4");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}

void test_sub_negative_numbers(void)
{
    BigInt *a = test_integer("-15");
    BigInt *b = test_integer("-11");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_sub(result, a, b));

    test_assert_integer(result, "-4");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}

void test_sub_alias(void)
{
    BigInt *a = test_integer("15");
    BigInt *b = test_integer("11");

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_sub(a, a, b));

    test_assert_integer(a, "4");

    bigint_destroy(a);
    bigint_destroy(b);
}

/* MUL */

void test_mul_positive(void)
{
    BigInt *a = test_integer("12");
    BigInt *b = test_integer("13");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_mul(result, a, b));

    test_assert_integer(result, "156");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}

void test_mul_negative(void)
{
    BigInt *a = test_integer("-12");
    BigInt *b = test_integer("13");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_mul(result, a, b));

    test_assert_integer(result, "-156");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}

void test_mul_negative_negative(void)
{
    BigInt *a = test_integer("-12");
    BigInt *b = test_integer("-13");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_mul(result, a, b));

    test_assert_integer(result, "156");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}

void test_mul_zero(void)
{
    BigInt *a = test_integer("123456789");
    BigInt *b = test_integer("0");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_mul(result, a, b));

    test_assert_integer(result, "0");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}

void test_mul_alias_all(void)
{
    BigInt *a = test_integer("123");

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_mul(a, a, a));

    test_assert_integer(a, "15129");

    bigint_destroy(a);
}

void test_mul_multi_limb(void)
{
    BigInt *a = test_integer("18446744073709551616");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_mul(result, a, a));
    test_assert_integer(result, "340282366920938463463374607431768211456");

    bigint_destroy(a);
    bigint_destroy(result);
}

/* DIV / MOD */

void test_div_positive(void)
{
    BigInt *a = test_integer("100");
    BigInt *b = test_integer("7");
    BigInt *q = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_div(q, a, b));

    test_assert_integer(q, "14");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(q);
}

void test_div_negative(void)
{
    BigInt *a = test_integer("-100");
    BigInt *b = test_integer("7");
    BigInt *q = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_div(q, a, b));

    test_assert_integer(q, "-14");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(q);
}

void test_div_negative_divisor(void)
{
    BigInt *a = test_integer("100");
    BigInt *b = test_integer("-7");
    BigInt *q = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_div(q, a, b));

    test_assert_integer(q, "-14");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(q);
}

void test_division_by_zero(void)
{
    BigInt *a = test_integer("100");
    BigInt *b = test_integer("0");
    BigInt *q = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_DIVISION_BY_ZERO,
        bigint_div(q, a, b)
    );

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(q);
}

void test_mod_positive(void)
{
    BigInt *a = test_integer("100");
    BigInt *b = test_integer("7");
    BigInt *r = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_mod(r, a, b));

    test_assert_integer(r, "2");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(r);
}

void test_mod_negative(void)
{
    BigInt *a = test_integer("-100");
    BigInt *b = test_integer("7");
    BigInt *r = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_mod(r, a, b));

    /* C-style remainder: sign follows dividend */
    test_assert_integer(r, "-2");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(r);
}

void test_division_with_identical_input_pointers(void)
{
    for (unsigned alias = 0; alias < 3U; alias++)
    {
        BigInt *value = test_integer("-340282366920938463463374607431768211457");
        BigInt *original_q = test_integer("111"), *original_r = test_integer("222");
        BigInt *q = alias == 1U ? value : original_q;
        BigInt *r = alias == 2U ? value : original_r;
        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_div_mod(q, r, value, value));
        test_assert_integer(q, "1");
        test_assert_integer(r, "0");
        if (alias == 0U) test_assert_integer(value, "-340282366920938463463374607431768211457");
        bigint_destroy(value); bigint_destroy(original_q); bigint_destroy(original_r);
    }
}

void test_div_mod(void)
{
    BigInt *a = test_integer("100");
    BigInt *b = test_integer("7");
    BigInt *q = bigint_create();
    BigInt *r = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_div_mod(q, r, a, b)
    );

    test_assert_integer(q, "14");
    test_assert_integer(r, "2");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(q);
    bigint_destroy(r);
}

void test_div_mod_negative(void)
{
    BigInt *a = test_integer("-100");
    BigInt *b = test_integer("7");
    BigInt *q = bigint_create();
    BigInt *r = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_div_mod(q, r, a, b)
    );

    test_assert_integer(q, "-14");
    test_assert_integer(r, "-2");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(q);
    bigint_destroy(r);
}

void test_div_mod_same_output(void)
{
    BigInt *a = test_integer("100");
    BigInt *b = test_integer("7");
    BigInt *zero = test_integer("0");
    BigInt *x = test_integer("999");

    TEST_ASSERT_EQUAL(
        BIGINT_INVALID_ARGUMENT,
        bigint_div_mod(x, x, a, b)
    );

    /* Must not modify x */
    test_assert_integer(x, "999");

    TEST_ASSERT_EQUAL(
        BIGINT_INVALID_ARGUMENT,
        bigint_div_mod(x, x, a, zero)
    );
    test_assert_integer(x, "999");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(zero);
    bigint_destroy(x);
}

void test_div_alias_quotient(void)
{
    BigInt *a = test_integer("100");
    BigInt *b = test_integer("7");

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_div(a, a, b)
    );

    test_assert_integer(a, "14");

    bigint_destroy(a);
    bigint_destroy(b);
}

void test_mod_alias_remainder(void)
{
    BigInt *a = test_integer("100");
    BigInt *b = test_integer("7");

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_mod(a, a, b)
    );

    test_assert_integer(a, "2");

    bigint_destroy(a);
    bigint_destroy(b);
}

/* POW */

void test_pow_zero_exponent(void)
{
    BigInt *base = test_integer("12345");
    BigInt *exponent = test_integer("0");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_pow(result, base, exponent)
    );

    test_assert_integer(result, "1");

    bigint_destroy(base);
    bigint_destroy(exponent);
    bigint_destroy(result);
}

void test_pow_positive(void)
{
    BigInt *base = test_integer("2");
    BigInt *exponent = test_integer("10");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_pow(result, base, exponent)
    );

    test_assert_integer(result, "1024");

    bigint_destroy(base);
    bigint_destroy(exponent);
    bigint_destroy(result);
}

void test_pow_negative_base_even(void)
{
    BigInt *base = test_integer("-2");
    BigInt *exponent = test_integer("10");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_pow(result, base, exponent)
    );

    test_assert_integer(result, "1024");

    bigint_destroy(base);
    bigint_destroy(exponent);
    bigint_destroy(result);
}

void test_pow_negative_base_odd(void)
{
    BigInt *base = test_integer("-2");
    BigInt *exponent = test_integer("9");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_pow(result, base, exponent)
    );

    test_assert_integer(result, "-512");

    bigint_destroy(base);
    bigint_destroy(exponent);
    bigint_destroy(result);
}

void test_pow_zero_base(void)
{
    BigInt *base = test_integer("0");
    BigInt *exponent = test_integer("5");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_pow(result, base, exponent)
    );

    test_assert_integer(result, "0");

    bigint_destroy(base);
    bigint_destroy(exponent);
    bigint_destroy(result);
}

void test_pow_large(void)
{
    BigInt *base = test_integer("900");
    BigInt *exponent = test_integer("900");
    BigInt *result = bigint_create();

    TEST_ASSERT_NOT_NULL(base);
    TEST_ASSERT_NOT_NULL(exponent);
    TEST_ASSERT_NOT_NULL(result);

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_pow(result, base, exponent)
    );

    char *string = bigint_to_string(result);

    TEST_ASSERT_NOT_NULL(string);

    /* 900^900 has exactly 2659 decimal digits. */
    TEST_ASSERT_EQUAL(
        2659,
        strlen(string)
    );

    /* Verify the beginning of the result. */
    TEST_ASSERT_EQUAL_STRING(
        "6580493968408632804929014571610917550071248571219292334870797716335502409780715948183482636992699707424165543395059496157527833505163485814780832508280321982693193894021693760105265651096502487787465992443912225654648394809985020018896847966262304808104450951459917271929335121077825606432856463873895111877418193250368523969606686048354063473475613354161885741044613517295232422038239661979580245371849813440987810956993121482523331697646057634218168344407060391273557651616218476077103109748770902844693502522334530055012826919209455732261178347117269890804977527907780783936724643978636930179531159122524466991097474116984858564114050431421671051677196843425483621454307154150865549579965933340401668320980048649612775610948840953267331272197797265622887142620061832365829164273328524847448583858058718839294294547098355245656095552973992113091419987396001000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
        string
    );

    free(string);

    bigint_destroy(base);
    bigint_destroy(exponent);
    bigint_destroy(result);
}

void test_pow_zero_zero(void)
{
    BigInt *base = test_integer("0");
    BigInt *exponent = test_integer("0");
    BigInt *result = bigint_create();

    TEST_ASSERT_NOT_NULL(base);
    TEST_ASSERT_NOT_NULL(exponent);
    TEST_ASSERT_NOT_NULL(result);

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_pow(result, base, exponent)
    );

    char *string = bigint_to_string(result);

    TEST_ASSERT_NOT_NULL(string);

    TEST_ASSERT_EQUAL_STRING(
        "1",
        string
    );

    free(string);
    bigint_destroy(base);
    bigint_destroy(exponent);
    bigint_destroy(result);
}

void test_pow_negative_exponent(void)
{
    BigInt *base = test_integer("2");
    BigInt *exponent = test_integer("-1");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_NEGATIVE_ARGUMENT,
        bigint_pow(result, base, exponent)
    );

    bigint_destroy(base);
    bigint_destroy(exponent);
    bigint_destroy(result);
}

void test_pow_supports_all_output_input_aliases(void)
{
    BigInt *base = test_integer("-2");
    BigInt *exponent = test_integer("3");
    BigInt *same = test_integer("2");

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_pow(base, base, exponent));
    test_assert_integer(base, "-8");

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(base, "-2"));
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_pow(exponent, base, exponent));
    test_assert_integer(exponent, "-8");

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_pow(same, same, same));
    test_assert_integer(same, "4");

    bigint_destroy(base);
    bigint_destroy(exponent);
    bigint_destroy(same);
}

/* GCD */

void test_gcd_basic(void)
{
    BigInt *a = test_integer("48");
    BigInt *b = test_integer("18");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_gcd(result, a, b)
    );

    test_assert_integer(result, "6");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}

void test_gcd_coprime(void)
{
    BigInt *a = test_integer("17");
    BigInt *b = test_integer("13");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_gcd(result, a, b)
    );

    test_assert_integer(result, "1");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}

void test_gcd_negative(void)
{
    BigInt *a = test_integer("-48");
    BigInt *b = test_integer("18");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_gcd(result, a, b)
    );

    test_assert_integer(result, "6");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}

void test_gcd_zero(void)
{
    BigInt *a = test_integer("48");
    BigInt *b = test_integer("0");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_gcd(result, a, b)
    );

    test_assert_integer(result, "48");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}

/* LCM */

void test_lcm_basic(void)
{
    BigInt *a = test_integer("12");
    BigInt *b = test_integer("18");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_lcm(result, a, b)
    );

    test_assert_integer(result, "36");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}

void test_lcm_negative(void)
{
    BigInt *a = test_integer("-12");
    BigInt *b = test_integer("18");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_lcm(result, a, b)
    );

    test_assert_integer(result, "36");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}

void test_lcm_zero(void)
{
    BigInt *a = test_integer("12");
    BigInt *b = test_integer("0");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_lcm(result, a, b)
    );

    test_assert_integer(result, "0");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}

/* FACTORIAL */

void test_factorial_group_boundaries(void)
{
    static const struct { const char *n; const char *expected; } cases[] = {
        { "19", "121645100408832000" },
        { "20", "2432902008176640000" },
        { "21", "51090942171709440000" },
        { "22", "1124000727777607680000" },
        { "50", "30414093201713378043612608166064768844377641568960512000000000000" }
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        BigInt *value = test_integer(cases[i].n);
        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_factorial(value, value));
        test_assert_integer(value, cases[i].expected);
        bigint_destroy(value);
    }
}

void test_factorial_zero(void)
{
    BigInt *n = test_integer("0");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_factorial(result, n)
    );

    test_assert_integer(result, "1");

    bigint_destroy(n);
    bigint_destroy(result);
}

void test_factorial_one(void)
{
    BigInt *n = test_integer("1");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_factorial(result, n)
    );

    test_assert_integer(result, "1");

    bigint_destroy(n);
    bigint_destroy(result);
}

void test_factorial_five(void)
{
    BigInt *n = test_integer("5");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_factorial(result, n)
    );

    test_assert_integer(result, "120");

    bigint_destroy(n);
    bigint_destroy(result);
}

void test_factorial_ten(void)
{
    BigInt *n = test_integer("10");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_factorial(result, n)
    );

    test_assert_integer(result, "3628800");

    bigint_destroy(n);
    bigint_destroy(result);
}

void test_factorial_negative(void)
{
    BigInt *n = test_integer("-1");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_NEGATIVE_ARGUMENT,
        bigint_factorial(result, n)
    );

    bigint_destroy(n);
    bigint_destroy(result);
}

void test_factorial_overflow(void)
{
    BigInt *n = test_integer("18446744073709551616"); // 2^64
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_VALUE_TOO_LARGE, bigint_factorial(result, n));

    bigint_destroy(n);
    bigint_destroy(result);
}

void test_factorial_alias_input(void)
{
    BigInt *value = test_integer("10");

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_factorial(value, value));
    test_assert_integer(value, "3628800");

    bigint_destroy(value);
}

void test_factorial_above_counter_range(void)
{
    BigInt *n = test_integer("18446744073709551616");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_VALUE_TOO_LARGE, bigint_factorial(result, n));

    bigint_destroy(n);
    bigint_destroy(result);
}

/* PERMUTATIONS / COMBINATIONS */

void test_permutations_and_combinations(void)
{
    static const struct
    {
        const char *n;
        const char *r;
        const char *permutation;
        const char *combination;
    } cases[] = {
        { "0", "0", "1", "1" },
        { "5", "0", "1", "1" },
        { "5", "2", "20", "10" },
        { "5", "5", "120", "1" },
        { "20", "5", "1860480", "15504" },
        { "52", "5", "311875200", "2598960" },
        /* Independent closed-form n(n-1)/2 and n(n-1)(n-2)/6 references. */
        { "18446744073709551615", "2",
          "340282366920938463408034375210639556610",
          "170141183460469231704017187605319778305" },
        { "18446744073709551616", "2",
          "340282366920938463444927863358058659840",
          "170141183460469231722463931679029329920" },
        { "18446744073709551617", "3",
          "6277101735386680763835789423207666416083908700390324961280",
          "1046183622564446793972631570534611069347318116731720826880" },
        { "100", "50",
          "3068518756254966037202730459529469739228459721684688959447786986982158958772355072000000000000",
          "100891344545564193334812497256" }
    };

    for (size_t index = 0U; index < sizeof(cases) / sizeof(cases[0]); index++)
    {
        BigInt *n = test_integer(cases[index].n);
        BigInt *r = test_integer(cases[index].r);
        BigInt *result = bigint_create();

        TEST_ASSERT_NOT_NULL(result);
        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_permutation(result, n, r));
        test_assert_integer(result, cases[index].permutation);
        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_combination(result, n, r));
        test_assert_integer(result, cases[index].combination);

        bigint_destroy(n);
        bigint_destroy(r);
        bigint_destroy(result);
    }
}

void test_combinatorics_aliasing_and_domains(void)
{
    BigInt *n = test_integer("10");
    BigInt *r = test_integer("3");
    BigInt *negative = test_integer("-1");
    BigInt *too_large = test_integer("11");
    BigInt *result = test_integer("42");

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_permutation(n, n, r));
    test_assert_integer(n, "720");
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_combination(r, too_large, r));
    test_assert_integer(r, "165");

    TEST_ASSERT_EQUAL(BIGINT_NEGATIVE_ARGUMENT,
                      bigint_permutation(result, negative, r));
    test_assert_integer(result, "42");
    TEST_ASSERT_EQUAL(BIGINT_NEGATIVE_ARGUMENT,
                      bigint_combination(result, too_large, negative));
    test_assert_integer(result, "42");
    TEST_ASSERT_EQUAL(BIGINT_INVALID_ARGUMENT,
                      bigint_combination(result, too_large, n));
    test_assert_integer(result, "42");

    bigint_destroy(n);
    bigint_destroy(r);
    bigint_destroy(negative);
    bigint_destroy(too_large);
    bigint_destroy(result);
}

/* BITWISE AND */

void test_and_basic(void)
{
    BigInt *a = test_integer("12"); /* 1100 */
    BigInt *b = test_integer("10"); /* 1010 */
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_and(result, a, b)
    );

    test_assert_integer(result, "8");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}

void test_and_zero(void)
{
    BigInt *a = test_integer("123");
    BigInt *b = test_integer("0");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_and(result, a, b)
    );

    test_assert_integer(result, "0");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}

void test_and_negative_argument(void)
{
    BigInt *a = test_integer("-1");
    BigInt *b = test_integer("10");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_NEGATIVE_ARGUMENT,
        bigint_and(result, a, b)
    );

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}

/* BITWISE OR */

void test_or_basic(void)
{
    BigInt *a = test_integer("12"); /* 1100 */
    BigInt *b = test_integer("10"); /* 1010 */
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_or(result, a, b)
    );

    test_assert_integer(result, "14");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}

void test_or_zero(void)
{
    BigInt *a = test_integer("123");
    BigInt *b = test_integer("0");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_or(result, a, b)
    );

    test_assert_integer(result, "123");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}

void test_or_negative_argument(void)
{
    BigInt *a = test_integer("-1");
    BigInt *b = test_integer("10");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_NEGATIVE_ARGUMENT,
        bigint_or(result, a, b)
    );

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}

/* BITWISE XOR */

void test_xor_basic(void)
{
    BigInt *a = test_integer("12"); /* 1100 */
    BigInt *b = test_integer("10"); /* 1010 */
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_xor(result, a, b)
    );

    test_assert_integer(result, "6");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}

void test_xor_same_value(void)
{
    BigInt *a = test_integer("123456");
    BigInt *b = test_integer("123456");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_xor(result, a, b)
    );

    test_assert_integer(result, "0");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}

void test_xor_negative_argument(void)
{
    BigInt *a = test_integer("-1");
    BigInt *b = test_integer("10");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_NEGATIVE_ARGUMENT,
        bigint_xor(result, a, b)
    );

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}

/* BITWISE NOT */

void test_not_zero(void)
{
    BigInt *a = test_integer("0");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_not(result, a)
    );

    test_assert_integer(result, "-1");

    bigint_destroy(a);
    bigint_destroy(result);
}

void test_not_positive(void)
{
    BigInt *a = test_integer("5");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_not(result, a)
    );

    /* ~(5) = -(5 + 1) = -6 */

    test_assert_integer(result, "-6");

    bigint_destroy(a);
    bigint_destroy(result);
}

void test_not_negative(void)
{
    BigInt *a = test_integer("-5");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_not(result, a)
    );

    test_assert_integer(result, "4");

    bigint_destroy(a);
    bigint_destroy(result);
}

/* SHIFT LEFT */

void test_shift_left_zero(void)
{
    BigInt *a = test_integer("123");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_shift_left(result, a, 0)
    );

    test_assert_integer(result, "123");

    bigint_destroy(a);
    bigint_destroy(result);
}

void test_shift_left_basic(void)
{
    BigInt *a = test_integer("5");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_shift_left(result, a, 3)
    );

    test_assert_integer(result, "40");

    bigint_destroy(a);
    bigint_destroy(result);
}

void test_shift_left_large(void)
{
    BigInt *a = test_integer("1");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_shift_left(result, a, 100)
    );

    /*
       2^100
       = 1267650600228229401496703205376
    */

    test_assert_integer(
        result,
        "1267650600228229401496703205376"
    );

    bigint_destroy(a);
    bigint_destroy(result);
}

void test_shift_left_negative(void)
{
    BigInt *a = test_integer("-5");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_shift_left(result, a, 3)
    );

    test_assert_integer(result, "-40");

    bigint_destroy(a);
    bigint_destroy(result);
}

/* SHIFT RIGHT */

void test_shift_right_zero(void)
{
    BigInt *a = test_integer("123");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_shift_right(result, a, 0)
    );

    test_assert_integer(result, "123");

    bigint_destroy(a);
    bigint_destroy(result);
}

void test_shift_right_basic(void)
{
    BigInt *a = test_integer("40");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_shift_right(result, a, 3)
    );

    test_assert_integer(result, "5");

    bigint_destroy(a);
    bigint_destroy(result);
}

void test_shift_right_truncation(void)
{
    BigInt *a = test_integer("43");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_shift_right(result, a, 3)
    );

    test_assert_integer(result, "5");

    bigint_destroy(a);
    bigint_destroy(result);
}

void test_shift_right_negative(void)
{
    BigInt *a = test_integer("-43");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_shift_right(result, a, 3)
    );

    /* Truncating toward zero: -43 / 8 = -5 */

    test_assert_integer(result, "-5");

    bigint_destroy(a);
    bigint_destroy(result);
}

void test_shift_right_too_far(void)
{
    BigInt *a = test_integer("123");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_shift_right(result, a, 1000)
    );

    test_assert_integer(result, "0");

    bigint_destroy(a);
    bigint_destroy(result);
}

/* EVEN / ODD */

void test_is_even(void)
{
    BigInt *even = test_integer("100");
    BigInt *odd = test_integer("101");

    TEST_ASSERT_TRUE(bigint_is_even(even));
    TEST_ASSERT_FALSE(bigint_is_even(odd));

    bigint_destroy(even);
    bigint_destroy(odd);
}

void test_is_odd(void)
{
    BigInt *even = test_integer("100");
    BigInt *odd = test_integer("101");

    TEST_ASSERT_FALSE(bigint_is_odd(even));
    TEST_ASSERT_TRUE(bigint_is_odd(odd));

    bigint_destroy(even);
    bigint_destroy(odd);
}

void test_even_odd_zero(void)
{
    BigInt *zero = test_integer("0");

    TEST_ASSERT_TRUE(bigint_is_even(zero));
    TEST_ASSERT_FALSE(bigint_is_odd(zero));

    bigint_destroy(zero);
}

void test_even_odd_negative(void)
{
    BigInt *even = test_integer("-100");
    BigInt *odd = test_integer("-101");

    TEST_ASSERT_TRUE(bigint_is_even(even));
    TEST_ASSERT_TRUE(bigint_is_odd(odd));

    bigint_destroy(even);
    bigint_destroy(odd);
}

/* PROBABLE PRIME */

void test_prime_small_primes(void)
{
    BigInt *p2 = test_integer("2");
    BigInt *p3 = test_integer("3");
    BigInt *p17 = test_integer("17");
    BigInt *p97 = test_integer("97");

    assert_bigint_predicate(bigint_is_probable_prime, true, p2);
    assert_bigint_predicate(bigint_is_probable_prime, true, p3);
    assert_bigint_predicate(bigint_is_probable_prime, true, p17);
    assert_bigint_predicate(bigint_is_probable_prime, true, p97);

    bigint_destroy(p2);
    bigint_destroy(p3);
    bigint_destroy(p17);
    bigint_destroy(p97);
}

void test_prime_composites(void)
{
    BigInt *p0 = test_integer("0");
    BigInt *p1 = test_integer("1");
    BigInt *p4 = test_integer("4");
    BigInt *p15 = test_integer("15");
    BigInt *p100 = test_integer("100");

    assert_bigint_predicate(bigint_is_probable_prime, false, p0);
    assert_bigint_predicate(bigint_is_probable_prime, false, p1);
    assert_bigint_predicate(bigint_is_probable_prime, false, p4);
    assert_bigint_predicate(bigint_is_probable_prime, false, p15);
    assert_bigint_predicate(bigint_is_probable_prime, false, p100);

    bigint_destroy(p0);
    bigint_destroy(p1);
    bigint_destroy(p4);
    bigint_destroy(p15);
    bigint_destroy(p100);
}

void test_prime_composite_without_small_factor(void)
{
    /* 3000017 and 3000029 are prime, so this composite has no factor
       within the former 3,000,000 trial-division limit. */
    BigInt *x = test_integer("9000138000493");

    assert_bigint_predicate(bigint_is_probable_prime, false, x);

    bigint_destroy(x);
}

void test_prime_multi_limb_values(void)
{
    /* 2^127 - 1 is prime; its square exercises the multi-limb composite path. */
    BigInt *prime = test_integer("170141183460469231731687303715884105727");
    BigInt *composite = test_integer(
        "28948022309329048855892746252171976962977213799489202546401021394546514198529"
    );

    assert_bigint_predicate(bigint_is_probable_prime, true, prime);
    assert_bigint_predicate(bigint_is_probable_prime, false, composite);

    bigint_destroy(prime);
    bigint_destroy(composite);
}

void test_prime_negative(void)
{
    BigInt *x = test_integer("-17");

    assert_bigint_predicate(bigint_is_probable_prime, false, x);

    bigint_destroy(x);
}

/* PERFECT SQUARE */

void test_perfect_square_zero(void)
{
    BigInt *x = test_integer("0");

    assert_bigint_predicate(bigint_is_perfect_square, true, x);

    bigint_destroy(x);
}

void test_perfect_square_one(void)
{
    BigInt *x = test_integer("1");

    assert_bigint_predicate(bigint_is_perfect_square, true, x);

    bigint_destroy(x);
}

void test_perfect_square_positive(void)
{
    BigInt *x = test_integer("144");

    assert_bigint_predicate(bigint_is_perfect_square, true, x);

    bigint_destroy(x);
}

void test_perfect_square_large(void)
{
    BigInt *x = test_integer(
        "15241578750190521"
    );

    /*
       123456789^2
       = 15241578750190521
    */

    assert_bigint_predicate(bigint_is_perfect_square, true, x);

    bigint_destroy(x);
}

void test_perfect_square_multi_limb_values(void)
{
    BigInt *square = test_integer(
        "28948022309329048855892746252171976962977213799489202546401021394546514198529"
    );
    BigInt *previous = test_integer(
        "28948022309329048855892746252171976962977213799489202546401021394546514198528"
    );

    assert_bigint_predicate(bigint_is_perfect_square, true, square);
    assert_bigint_predicate(bigint_is_perfect_square, false, previous);

    bigint_destroy(square);
    bigint_destroy(previous);
}

void test_not_perfect_square(void)
{
    BigInt *x = test_integer("145");

    assert_bigint_predicate(bigint_is_perfect_square, false, x);

    bigint_destroy(x);
}

void test_negative_not_perfect_square(void)
{
    BigInt *x = test_integer("-144");

    assert_bigint_predicate(bigint_is_perfect_square, false, x);

    bigint_destroy(x);
}

void test_number_theory_predicate_null_arguments(void)
{
    BigInt *value = test_integer("17");
    bool result = true;

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_is_probable_prime(NULL, value)
    );
    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_is_probable_prime(&result, NULL)
    );
    TEST_ASSERT_TRUE(result);

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_is_perfect_square(NULL, value)
    );
    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_is_perfect_square(&result, NULL)
    );
    TEST_ASSERT_TRUE(result);

    bigint_destroy(value);
}

/* NULL argument tests */

void test_arithmetic_null_arguments(void)
{
    BigInt *a = test_integer("10");
    BigInt *b = test_integer("5");
    BigInt *r = bigint_create();
    BigInt *remainder = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_abs(NULL, a)
    );

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_abs(r, NULL)
    );

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_negate(NULL, a)
    );

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_negate(r, NULL)
    );

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_add(NULL, a, b)
    );

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_add(r, NULL, b)
    );

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_add(r, a, NULL)
    );

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_sub(NULL, a, b)
    );

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_mul(NULL, a, b)
    );

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_div(NULL, a, b)
    );

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_mod(NULL, a, b)
    );

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_div_mod(NULL, r, a, b)
    );

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_div_mod(r, NULL, a, b)
    );

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_div_mod(r, r, NULL, b)
    );

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_div_mod(r, remainder, a, NULL)
    );

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_pow(NULL, a, b)
    );

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_gcd(NULL, a, b)
    );

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_lcm(NULL, a, b)
    );

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_factorial(NULL, a)
    );

    TEST_ASSERT_EQUAL(BIGINT_NULL_ARGUMENT, bigint_permutation(NULL, a, b));
    TEST_ASSERT_EQUAL(BIGINT_NULL_ARGUMENT, bigint_permutation(r, NULL, b));
    TEST_ASSERT_EQUAL(BIGINT_NULL_ARGUMENT, bigint_permutation(r, a, NULL));
    TEST_ASSERT_EQUAL(BIGINT_NULL_ARGUMENT, bigint_combination(NULL, a, b));
    TEST_ASSERT_EQUAL(BIGINT_NULL_ARGUMENT, bigint_combination(r, NULL, b));
    TEST_ASSERT_EQUAL(BIGINT_NULL_ARGUMENT, bigint_combination(r, a, NULL));

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_and(NULL, a, b)
    );

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_or(NULL, a, b)
    );

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_xor(NULL, a, b)
    );

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_not(NULL, a)
    );

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_shift_left(NULL, a, 1)
    );

    TEST_ASSERT_EQUAL(
        BIGINT_NULL_ARGUMENT,
        bigint_shift_right(NULL, a, 1)
    );

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(r);
    bigint_destroy(remainder);
}

/* Main */

static void test_base_conversion_contract(void)
{
    BigInt *value = test_integer(base_reference_decimal);
    BigInt *parsed = test_integer("777");
    char *text = NULL;
    char marker[] = "unchanged";
    for (size_t i=0; i<sizeof(base_references)/sizeof(base_references[0]); i++)
    {
        unsigned base = base_references[i].base;
        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_to_string_base(value, base, false, &text));
        TEST_ASSERT_EQUAL_STRING(base_references[i].text, text);
        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string_base(parsed, text, base));
        test_assert_integer(parsed, base_reference_decimal);
        free(text); text = NULL;
    }
    const char *invalid[] = {"", "+", "-", " 12", "12 ", "0xFF", "1_0", "G"};
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(parsed, "777"));
    for(size_t i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++)
    {
        TEST_ASSERT_EQUAL(BIGINT_INVALID_ARGUMENT, bigint_set_string_base(parsed, invalid[i], 16));
        test_assert_integer(parsed,"777");
    }
    text=marker;
    TEST_ASSERT_EQUAL(BIGINT_INVALID_ARGUMENT,bigint_to_string_base(value,1,true,&text));
    TEST_ASSERT_EQUAL_PTR(marker,text);
    TEST_ASSERT_EQUAL(BIGINT_INVALID_ARGUMENT,bigint_set_string_base(parsed,"10",37));
    TEST_ASSERT_EQUAL(BIGINT_NULL_ARGUMENT,bigint_set_string_base(NULL,"10",2));
    TEST_ASSERT_EQUAL(BIGINT_NULL_ARGUMENT,bigint_to_string_base(value,2,false,NULL));
    TEST_ASSERT_EQUAL(BIGINT_OK,bigint_set_string_base(parsed,"-000",36));
    TEST_ASSERT_EQUAL(BIGINT_OK,bigint_to_string_base(parsed,36,true,&text));
    TEST_ASSERT_EQUAL_STRING("0",text); free(text);
    TEST_ASSERT_EQUAL(BIGINT_OK,bigint_set_string_base(parsed,"+000fF",16));
    test_assert_integer(parsed,"255");
    bigint_destroy(value);bigint_destroy(parsed);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_base_conversion_contract);

    /* Status */
    RUN_TEST(test_status_to_string);

    /* Create / destroy */
    RUN_TEST(test_create_destroy);
    RUN_TEST(test_destroy_null);

    /* String */
    RUN_TEST(test_set_string_zero);
    RUN_TEST(test_set_string_positive);
    RUN_TEST(test_set_string_negative);
    RUN_TEST(test_set_string_plus_sign);
    RUN_TEST(test_set_string_large_number);
    RUN_TEST(test_set_string_leading_zeros);
    RUN_TEST(test_set_string_negative_zero);
    RUN_TEST(test_set_string_empty);
    RUN_TEST(test_set_string_sign_only);
    RUN_TEST(test_set_string_invalid_characters);
    RUN_TEST(test_set_string_null_arguments);
    RUN_TEST(test_to_string_null);

    /* Copy */
    RUN_TEST(test_copy);
    RUN_TEST(test_copy_self);
    RUN_TEST(test_copy_null_arguments);

    /* Comparison */
    RUN_TEST(test_compare_equal);
    RUN_TEST(test_compare_less);
    RUN_TEST(test_compare_greater);
    RUN_TEST(test_compare_negative_numbers);
    RUN_TEST(test_compare_negative_positive);

    /* Properties */
    RUN_TEST(test_is_zero);
    RUN_TEST(test_is_one);
    RUN_TEST(test_is_negative);

    /* ABS / negate */
    RUN_TEST(test_abs_positive);
    RUN_TEST(test_abs_negative);
    RUN_TEST(test_abs_zero);
    RUN_TEST(test_negate_positive);
    RUN_TEST(test_negate_negative);
    RUN_TEST(test_negate_zero);

    /* Addition */
    RUN_TEST(test_add_positive);
    RUN_TEST(test_add_negative);
    RUN_TEST(test_add_mixed_signs);
    RUN_TEST(test_add_zero);
    RUN_TEST(test_add_across_limb_boundary);
    RUN_TEST(test_add_alias_left);
    RUN_TEST(test_add_alias_right);
    RUN_TEST(test_add_alias_all);

    /* Subtraction */
    RUN_TEST(test_sub_positive);
    RUN_TEST(test_sub_negative_result);
    RUN_TEST(test_sub_negative_numbers);
    RUN_TEST(test_sub_alias);

    /* Multiplication */
    RUN_TEST(test_mul_positive);
    RUN_TEST(test_mul_negative);
    RUN_TEST(test_mul_negative_negative);
    RUN_TEST(test_mul_zero);
    RUN_TEST(test_mul_alias_all);
    RUN_TEST(test_mul_multi_limb);

    /* Division / modulo */
    RUN_TEST(test_div_positive);
    RUN_TEST(test_div_negative);
    RUN_TEST(test_div_negative_divisor);
    RUN_TEST(test_division_by_zero);
    RUN_TEST(test_mod_positive);
    RUN_TEST(test_mod_negative);
    RUN_TEST(test_div_mod);
    RUN_TEST(test_division_with_identical_input_pointers);
    RUN_TEST(test_div_mod_negative);
    RUN_TEST(test_div_mod_same_output);
    RUN_TEST(test_div_alias_quotient);
    RUN_TEST(test_mod_alias_remainder);

    /* Power */
    RUN_TEST(test_pow_zero_exponent);
    RUN_TEST(test_pow_positive);
    RUN_TEST(test_pow_negative_base_even);
    RUN_TEST(test_pow_negative_base_odd);
    RUN_TEST(test_pow_zero_base);
    RUN_TEST(test_pow_zero_zero);
    RUN_TEST(test_pow_large);
    RUN_TEST(test_pow_negative_exponent);
    RUN_TEST(test_pow_supports_all_output_input_aliases);

    /* GCD */
    RUN_TEST(test_gcd_basic);
    RUN_TEST(test_gcd_coprime);
    RUN_TEST(test_gcd_negative);
    RUN_TEST(test_gcd_zero);

    /* LCM */
    RUN_TEST(test_lcm_basic);
    RUN_TEST(test_lcm_negative);
    RUN_TEST(test_lcm_zero);

    /* Factorial */
    RUN_TEST(test_factorial_zero);
    RUN_TEST(test_factorial_group_boundaries);
    RUN_TEST(test_factorial_one);
    RUN_TEST(test_factorial_five);
    RUN_TEST(test_factorial_ten);
    RUN_TEST(test_factorial_alias_input);
    RUN_TEST(test_factorial_negative);
    RUN_TEST(test_factorial_overflow);
    RUN_TEST(test_factorial_above_counter_range);

    /* Permutations and combinations */
    RUN_TEST(test_permutations_and_combinations);
    RUN_TEST(test_combinatorics_aliasing_and_domains);

    /* Bitwise */
    RUN_TEST(test_and_basic);
    RUN_TEST(test_and_zero);
    RUN_TEST(test_and_negative_argument);

    RUN_TEST(test_or_basic);
    RUN_TEST(test_or_zero);
    RUN_TEST(test_or_negative_argument);

    RUN_TEST(test_xor_basic);
    RUN_TEST(test_xor_same_value);
    RUN_TEST(test_xor_negative_argument);

    RUN_TEST(test_not_zero);
    RUN_TEST(test_not_positive);
    RUN_TEST(test_not_negative);

    /* Shifts */
    RUN_TEST(test_shift_left_zero);
    RUN_TEST(test_shift_left_basic);
    RUN_TEST(test_shift_left_large);
    RUN_TEST(test_shift_left_negative);

    RUN_TEST(test_shift_right_zero);
    RUN_TEST(test_shift_right_basic);
    RUN_TEST(test_shift_right_truncation);
    RUN_TEST(test_shift_right_negative);
    RUN_TEST(test_shift_right_too_far);

    /* Even / odd */
    RUN_TEST(test_is_even);
    RUN_TEST(test_is_odd);
    RUN_TEST(test_even_odd_zero);
    RUN_TEST(test_even_odd_negative);

    /* Prime */
    RUN_TEST(test_prime_small_primes);
    RUN_TEST(test_prime_composites);
    RUN_TEST(test_prime_composite_without_small_factor);
    RUN_TEST(test_prime_multi_limb_values);
    RUN_TEST(test_prime_negative);

    /* Perfect square */
    RUN_TEST(test_perfect_square_zero);
    RUN_TEST(test_perfect_square_one);
    RUN_TEST(test_perfect_square_positive);
    RUN_TEST(test_perfect_square_large);
    RUN_TEST(test_perfect_square_multi_limb_values);
    RUN_TEST(test_not_perfect_square);
    RUN_TEST(test_negative_not_perfect_square);
    RUN_TEST(test_number_theory_predicate_null_arguments);

    /* NULL arguments */
    RUN_TEST(test_arithmetic_null_arguments);

    return UNITY_END();
}
