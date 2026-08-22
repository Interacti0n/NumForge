#include <stdlib.h>
#include <string.h>

#include "unity.h"
#include "mysciencecalc/bigint.h"


/* ============================================================
   Test helpers
   ============================================================ */

static BigInt *make_bigint(const char *value)
{
    BigInt *x = bigint_create();

    TEST_ASSERT_NOT_NULL(x);
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(x, value));

    return x;
}


static void assert_bigint_string(const BigInt *value, const char *expected)
{
    char *actual = bigint_to_string(value);

    TEST_ASSERT_NOT_NULL(actual);
    TEST_ASSERT_EQUAL_STRING(expected, actual);

    free(actual);
}


static void assert_status(BigIntStatus actual, BigIntStatus expected)
{
    TEST_ASSERT_EQUAL(expected, actual);
}


/* ============================================================
   Unity setup / teardown
   ============================================================ */

void setUp(void)
{
}


void tearDown(void)
{
}


/* ============================================================
   Status
   ============================================================ */

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


/* ============================================================
   Create / destroy
   ============================================================ */

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


/* ============================================================
   set_string / to_string
   ============================================================ */

void test_set_string_zero(void)
{
    BigInt *x = make_bigint("0");

    assert_bigint_string(x, "0");

    bigint_destroy(x);
}


void test_set_string_positive(void)
{
    BigInt *x = make_bigint("123456789");

    assert_bigint_string(x, "123456789");

    bigint_destroy(x);
}


void test_set_string_negative(void)
{
    BigInt *x = make_bigint("-123456789");

    assert_bigint_string(x, "-123456789");

    bigint_destroy(x);
}


void test_set_string_plus_sign(void)
{
    BigInt *x = make_bigint("+123456789");

    assert_bigint_string(x, "123456789");

    bigint_destroy(x);
}


void test_set_string_large_number(void)
{
    BigInt *x = make_bigint(
        "12345678901234567890123456789012345678901234567890"
    );

    assert_bigint_string(
        x,
        "12345678901234567890123456789012345678901234567890"
    );

    bigint_destroy(x);
}


void test_set_string_leading_zeros(void)
{
    BigInt *x = make_bigint("0000000000012345");

    assert_bigint_string(x, "12345");

    bigint_destroy(x);
}


void test_set_string_negative_zero(void)
{
    BigInt *x = make_bigint("-0");

    assert_bigint_string(x, "0");
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


/* ============================================================
   Copy
   ============================================================ */

void test_copy(void)
{
    BigInt *source = make_bigint("-12345678901234567890");
    BigInt *destination = bigint_create();

    TEST_ASSERT_NOT_NULL(destination);

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_copy(destination, source)
    );

    assert_bigint_string(destination, "-12345678901234567890");

    bigint_destroy(source);
    bigint_destroy(destination);
}


void test_copy_self(void)
{
    BigInt *x = make_bigint("123456789");

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_copy(x, x)
    );

    assert_bigint_string(x, "123456789");

    bigint_destroy(x);
}


void test_copy_null_arguments(void)
{
    BigInt *x = make_bigint("123");

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


/* ============================================================
   Comparison
   ============================================================ */

void test_compare_equal(void)
{
    BigInt *a = make_bigint("12345");
    BigInt *b = make_bigint("12345");

    TEST_ASSERT_EQUAL(0, bigint_compare(a, b));

    bigint_destroy(a);
    bigint_destroy(b);
}


void test_compare_less(void)
{
    BigInt *a = make_bigint("12344");
    BigInt *b = make_bigint("12345");

    TEST_ASSERT_TRUE(bigint_compare(a, b) < 0);

    bigint_destroy(a);
    bigint_destroy(b);
}


void test_compare_greater(void)
{
    BigInt *a = make_bigint("12346");
    BigInt *b = make_bigint("12345");

    TEST_ASSERT_TRUE(bigint_compare(a, b) > 0);

    bigint_destroy(a);
    bigint_destroy(b);
}


void test_compare_negative_numbers(void)
{
    BigInt *a = make_bigint("-100");
    BigInt *b = make_bigint("-50");

    TEST_ASSERT_TRUE(bigint_compare(a, b) < 0);
    TEST_ASSERT_TRUE(bigint_compare(b, a) > 0);

    bigint_destroy(a);
    bigint_destroy(b);
}


void test_compare_negative_positive(void)
{
    BigInt *a = make_bigint("-1");
    BigInt *b = make_bigint("1");

    TEST_ASSERT_TRUE(bigint_compare(a, b) < 0);
    TEST_ASSERT_TRUE(bigint_compare(b, a) > 0);

    bigint_destroy(a);
    bigint_destroy(b);
}


/* ============================================================
   is_zero / is_one / is_negative
   ============================================================ */

void test_is_zero(void)
{
    BigInt *zero = make_bigint("0");
    BigInt *one = make_bigint("1");

    TEST_ASSERT_TRUE(bigint_is_zero(zero));
    TEST_ASSERT_FALSE(bigint_is_zero(one));

    bigint_destroy(zero);
    bigint_destroy(one);
}


void test_is_one(void)
{
    BigInt *one = make_bigint("1");
    BigInt *two = make_bigint("2");

    TEST_ASSERT_TRUE(bigint_is_one(one));
    TEST_ASSERT_FALSE(bigint_is_one(two));

    bigint_destroy(one);
    bigint_destroy(two);
}


void test_is_negative(void)
{
    BigInt *negative = make_bigint("-123");
    BigInt *positive = make_bigint("123");
    BigInt *zero = make_bigint("0");

    TEST_ASSERT_TRUE(bigint_is_negative(negative));
    TEST_ASSERT_FALSE(bigint_is_negative(positive));
    TEST_ASSERT_FALSE(bigint_is_negative(zero));

    bigint_destroy(negative);
    bigint_destroy(positive);
    bigint_destroy(zero);
}


/* ============================================================
   ABS
   ============================================================ */

void test_abs_positive(void)
{
    BigInt *a = make_bigint("123");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_abs(result, a)
    );

    assert_bigint_string(result, "123");

    bigint_destroy(a);
    bigint_destroy(result);
}


void test_abs_negative(void)
{
    BigInt *a = make_bigint("-123");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_abs(result, a)
    );

    assert_bigint_string(result, "123");

    bigint_destroy(a);
    bigint_destroy(result);
}


void test_abs_zero(void)
{
    BigInt *a = make_bigint("0");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_abs(result, a)
    );

    assert_bigint_string(result, "0");

    bigint_destroy(a);
    bigint_destroy(result);
}


/* ============================================================
   NEGATE
   ============================================================ */

void test_negate_positive(void)
{
    BigInt *a = make_bigint("123");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_negate(result, a)
    );

    assert_bigint_string(result, "-123");

    bigint_destroy(a);
    bigint_destroy(result);
}


void test_negate_negative(void)
{
    BigInt *a = make_bigint("-123");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_negate(result, a)
    );

    assert_bigint_string(result, "123");

    bigint_destroy(a);
    bigint_destroy(result);
}


void test_negate_zero(void)
{
    BigInt *a = make_bigint("0");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_negate(result, a)
    );

    assert_bigint_string(result, "0");

    bigint_destroy(a);
    bigint_destroy(result);
}


/* ============================================================
   ADD
   ============================================================ */

void test_add_positive(void)
{
    BigInt *a = make_bigint("15");
    BigInt *b = make_bigint("11");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_add(result, a, b));

    assert_bigint_string(result, "26");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}


void test_add_negative(void)
{
    BigInt *a = make_bigint("-15");
    BigInt *b = make_bigint("-11");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_add(result, a, b));

    assert_bigint_string(result, "-26");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}


void test_add_mixed_signs(void)
{
    BigInt *a = make_bigint("-15");
    BigInt *b = make_bigint("11");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_add(result, a, b));

    assert_bigint_string(result, "-4");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}


void test_add_zero(void)
{
    BigInt *a = make_bigint("123456789");
    BigInt *b = make_bigint("0");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_add(result, a, b));

    assert_bigint_string(result, "123456789");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}


void test_add_alias_left(void)
{
    BigInt *a = make_bigint("15");
    BigInt *b = make_bigint("11");

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_add(a, a, b));

    assert_bigint_string(a, "26");

    bigint_destroy(a);
    bigint_destroy(b);
}


void test_add_alias_right(void)
{
    BigInt *a = make_bigint("15");
    BigInt *b = make_bigint("11");

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_add(b, a, b));

    assert_bigint_string(b, "26");

    bigint_destroy(a);
    bigint_destroy(b);
}


void test_add_alias_all(void)
{
    BigInt *a = make_bigint("15");

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_add(a, a, a));

    assert_bigint_string(a, "30");

    bigint_destroy(a);
}


/* ============================================================
   SUB
   ============================================================ */

void test_sub_positive(void)
{
    BigInt *a = make_bigint("15");
    BigInt *b = make_bigint("11");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_sub(result, a, b));

    assert_bigint_string(result, "4");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}


void test_sub_negative_result(void)
{
    BigInt *a = make_bigint("11");
    BigInt *b = make_bigint("15");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_sub(result, a, b));

    assert_bigint_string(result, "-4");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}


void test_sub_negative_numbers(void)
{
    BigInt *a = make_bigint("-15");
    BigInt *b = make_bigint("-11");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_sub(result, a, b));

    assert_bigint_string(result, "-4");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}


void test_sub_alias(void)
{
    BigInt *a = make_bigint("15");
    BigInt *b = make_bigint("11");

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_sub(a, a, b));

    assert_bigint_string(a, "4");

    bigint_destroy(a);
    bigint_destroy(b);
}


/* ============================================================
   MUL
   ============================================================ */

void test_mul_positive(void)
{
    BigInt *a = make_bigint("12");
    BigInt *b = make_bigint("13");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_mul(result, a, b));

    assert_bigint_string(result, "156");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}


void test_mul_negative(void)
{
    BigInt *a = make_bigint("-12");
    BigInt *b = make_bigint("13");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_mul(result, a, b));

    assert_bigint_string(result, "-156");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}


void test_mul_negative_negative(void)
{
    BigInt *a = make_bigint("-12");
    BigInt *b = make_bigint("-13");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_mul(result, a, b));

    assert_bigint_string(result, "156");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}


void test_mul_zero(void)
{
    BigInt *a = make_bigint("123456789");
    BigInt *b = make_bigint("0");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_mul(result, a, b));

    assert_bigint_string(result, "0");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}


void test_mul_alias_all(void)
{
    BigInt *a = make_bigint("123");

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_mul(a, a, a));

    assert_bigint_string(a, "15129");

    bigint_destroy(a);
}


/* ============================================================
   DIV / MOD
   ============================================================ */

void test_div_positive(void)
{
    BigInt *a = make_bigint("100");
    BigInt *b = make_bigint("7");
    BigInt *q = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_div(q, a, b));

    assert_bigint_string(q, "14");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(q);
}


void test_div_negative(void)
{
    BigInt *a = make_bigint("-100");
    BigInt *b = make_bigint("7");
    BigInt *q = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_div(q, a, b));

    assert_bigint_string(q, "-14");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(q);
}


void test_div_negative_divisor(void)
{
    BigInt *a = make_bigint("100");
    BigInt *b = make_bigint("-7");
    BigInt *q = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_div(q, a, b));

    assert_bigint_string(q, "-14");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(q);
}


void test_division_by_zero(void)
{
    BigInt *a = make_bigint("100");
    BigInt *b = make_bigint("0");
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
    BigInt *a = make_bigint("100");
    BigInt *b = make_bigint("7");
    BigInt *r = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_mod(r, a, b));

    assert_bigint_string(r, "2");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(r);
}


void test_mod_negative(void)
{
    BigInt *a = make_bigint("-100");
    BigInt *b = make_bigint("7");
    BigInt *r = bigint_create();

    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_mod(r, a, b));

    /* C-style remainder: sign follows dividend */
    assert_bigint_string(r, "-2");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(r);
}


void test_div_mod(void)
{
    BigInt *a = make_bigint("100");
    BigInt *b = make_bigint("7");
    BigInt *q = bigint_create();
    BigInt *r = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_div_mod(q, r, a, b)
    );

    assert_bigint_string(q, "14");
    assert_bigint_string(r, "2");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(q);
    bigint_destroy(r);
}


void test_div_mod_negative(void)
{
    BigInt *a = make_bigint("-100");
    BigInt *b = make_bigint("7");
    BigInt *q = bigint_create();
    BigInt *r = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_div_mod(q, r, a, b)
    );

    assert_bigint_string(q, "-14");
    assert_bigint_string(r, "-2");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(q);
    bigint_destroy(r);
}


void test_div_mod_same_output(void)
{
    BigInt *a = make_bigint("100");
    BigInt *b = make_bigint("7");
    BigInt *x = make_bigint("999");

    TEST_ASSERT_EQUAL(
        BIGINT_INVALID_ARGUMENT,
        bigint_div_mod(x, x, a, b)
    );

    /* Must not modify x */
    assert_bigint_string(x, "999");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(x);
}


void test_div_alias_quotient(void)
{
    BigInt *a = make_bigint("100");
    BigInt *b = make_bigint("7");

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_div(a, a, b)
    );

    assert_bigint_string(a, "14");

    bigint_destroy(a);
    bigint_destroy(b);
}


void test_mod_alias_remainder(void)
{
    BigInt *a = make_bigint("100");
    BigInt *b = make_bigint("7");

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_mod(a, a, b)
    );

    assert_bigint_string(a, "2");

    bigint_destroy(a);
    bigint_destroy(b);
}


/* ============================================================
   POW
   ============================================================ */

void test_pow_zero_exponent(void)
{
    BigInt *base = make_bigint("12345");
    BigInt *exponent = make_bigint("0");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_pow(result, base, exponent)
    );

    assert_bigint_string(result, "1");

    bigint_destroy(base);
    bigint_destroy(exponent);
    bigint_destroy(result);
}


void test_pow_positive(void)
{
    BigInt *base = make_bigint("2");
    BigInt *exponent = make_bigint("10");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_pow(result, base, exponent)
    );

    assert_bigint_string(result, "1024");

    bigint_destroy(base);
    bigint_destroy(exponent);
    bigint_destroy(result);
}


void test_pow_negative_base_even(void)
{
    BigInt *base = make_bigint("-2");
    BigInt *exponent = make_bigint("10");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_pow(result, base, exponent)
    );

    assert_bigint_string(result, "1024");

    bigint_destroy(base);
    bigint_destroy(exponent);
    bigint_destroy(result);
}


void test_pow_negative_base_odd(void)
{
    BigInt *base = make_bigint("-2");
    BigInt *exponent = make_bigint("9");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_pow(result, base, exponent)
    );

    assert_bigint_string(result, "-512");

    bigint_destroy(base);
    bigint_destroy(exponent);
    bigint_destroy(result);
}


void test_pow_zero_base(void)
{
    BigInt *base = make_bigint("0");
    BigInt *exponent = make_bigint("5");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_pow(result, base, exponent)
    );

    assert_bigint_string(result, "0");

    bigint_destroy(base);
    bigint_destroy(exponent);
    bigint_destroy(result);
}


void test_pow_large_digits(void)
{
    BigInt *base = make_bigint("900");
    BigInt *exponent = make_bigint("900");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_pow(result, base, exponent)
    );

    /*
       We intentionally don't hardcode the 2659-digit result here.
       The purpose is to test that bigint_pow can calculate it
       successfully and produce a value with the expected size.
    */

    char *string = bigint_to_string(result);

    TEST_ASSERT_NOT_NULL(string);

    TEST_ASSERT_EQUAL(
        2659,
        strlen(string)
    );

    free(string);

    bigint_destroy(base);
    bigint_destroy(exponent);
    bigint_destroy(result);
}

void test_pow_large(void)
{
    BigInt *base = make_bigint("900");
    BigInt *exponent = make_bigint("900");
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

void test_pow_negative_exponent(void)
{
    BigInt *base = make_bigint("2");
    BigInt *exponent = make_bigint("-1");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_NEGATIVE_ARGUMENT,
        bigint_pow(result, base, exponent)
    );

    bigint_destroy(base);
    bigint_destroy(exponent);
    bigint_destroy(result);
}


/* ============================================================
   GCD
   ============================================================ */

void test_gcd_basic(void)
{
    BigInt *a = make_bigint("48");
    BigInt *b = make_bigint("18");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_gcd(result, a, b)
    );

    assert_bigint_string(result, "6");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}


void test_gcd_coprime(void)
{
    BigInt *a = make_bigint("17");
    BigInt *b = make_bigint("13");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_gcd(result, a, b)
    );

    assert_bigint_string(result, "1");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}


void test_gcd_negative(void)
{
    BigInt *a = make_bigint("-48");
    BigInt *b = make_bigint("18");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_gcd(result, a, b)
    );

    assert_bigint_string(result, "6");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}


void test_gcd_zero(void)
{
    BigInt *a = make_bigint("48");
    BigInt *b = make_bigint("0");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_gcd(result, a, b)
    );

    assert_bigint_string(result, "48");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}


/* ============================================================
   LCM
   ============================================================ */

void test_lcm_basic(void)
{
    BigInt *a = make_bigint("12");
    BigInt *b = make_bigint("18");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_lcm(result, a, b)
    );

    assert_bigint_string(result, "36");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}


void test_lcm_negative(void)
{
    BigInt *a = make_bigint("-12");
    BigInt *b = make_bigint("18");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_lcm(result, a, b)
    );

    assert_bigint_string(result, "36");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}


void test_lcm_zero(void)
{
    BigInt *a = make_bigint("12");
    BigInt *b = make_bigint("0");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_lcm(result, a, b)
    );

    assert_bigint_string(result, "0");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}


/* ============================================================
   FACTORIAL
   ============================================================ */

void test_factorial_zero(void)
{
    BigInt *n = make_bigint("0");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_factorial(result, n)
    );

    assert_bigint_string(result, "1");

    bigint_destroy(n);
    bigint_destroy(result);
}


void test_factorial_one(void)
{
    BigInt *n = make_bigint("1");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_factorial(result, n)
    );

    assert_bigint_string(result, "1");

    bigint_destroy(n);
    bigint_destroy(result);
}


void test_factorial_five(void)
{
    BigInt *n = make_bigint("5");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_factorial(result, n)
    );

    assert_bigint_string(result, "120");

    bigint_destroy(n);
    bigint_destroy(result);
}


void test_factorial_ten(void)
{
    BigInt *n = make_bigint("10");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_factorial(result, n)
    );

    assert_bigint_string(result, "3628800");

    bigint_destroy(n);
    bigint_destroy(result);
}


void test_factorial_negative(void)
{
    BigInt *n = make_bigint("-1");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_NEGATIVE_ARGUMENT,
        bigint_factorial(result, n)
    );

    bigint_destroy(n);
    bigint_destroy(result);
}


/* ============================================================
   BITWISE AND
   ============================================================ */

void test_and_basic(void)
{
    BigInt *a = make_bigint("12"); /* 1100 */
    BigInt *b = make_bigint("10"); /* 1010 */
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_and(result, a, b)
    );

    assert_bigint_string(result, "8");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}


void test_and_zero(void)
{
    BigInt *a = make_bigint("123");
    BigInt *b = make_bigint("0");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_and(result, a, b)
    );

    assert_bigint_string(result, "0");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}


void test_and_negative_argument(void)
{
    BigInt *a = make_bigint("-1");
    BigInt *b = make_bigint("10");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_NEGATIVE_ARGUMENT,
        bigint_and(result, a, b)
    );

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}


/* ============================================================
   BITWISE OR
   ============================================================ */

void test_or_basic(void)
{
    BigInt *a = make_bigint("12"); /* 1100 */
    BigInt *b = make_bigint("10"); /* 1010 */
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_or(result, a, b)
    );

    assert_bigint_string(result, "14");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}


void test_or_zero(void)
{
    BigInt *a = make_bigint("123");
    BigInt *b = make_bigint("0");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_or(result, a, b)
    );

    assert_bigint_string(result, "123");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}


void test_or_negative_argument(void)
{
    BigInt *a = make_bigint("-1");
    BigInt *b = make_bigint("10");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_NEGATIVE_ARGUMENT,
        bigint_or(result, a, b)
    );

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}


/* ============================================================
   BITWISE XOR
   ============================================================ */

void test_xor_basic(void)
{
    BigInt *a = make_bigint("12"); /* 1100 */
    BigInt *b = make_bigint("10"); /* 1010 */
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_xor(result, a, b)
    );

    assert_bigint_string(result, "6");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}


void test_xor_same_value(void)
{
    BigInt *a = make_bigint("123456");
    BigInt *b = make_bigint("123456");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_xor(result, a, b)
    );

    assert_bigint_string(result, "0");

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}


void test_xor_negative_argument(void)
{
    BigInt *a = make_bigint("-1");
    BigInt *b = make_bigint("10");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_NEGATIVE_ARGUMENT,
        bigint_xor(result, a, b)
    );

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}


/* ============================================================
   BITWISE NOT
   ============================================================ */

void test_not_zero(void)
{
    BigInt *a = make_bigint("0");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_not(result, a)
    );

    assert_bigint_string(result, "-1");

    bigint_destroy(a);
    bigint_destroy(result);
}


void test_not_positive(void)
{
    BigInt *a = make_bigint("5");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_not(result, a)
    );

    /* ~(5) = -(5 + 1) = -6 */

    assert_bigint_string(result, "-6");

    bigint_destroy(a);
    bigint_destroy(result);
}


void test_not_negative(void)
{
    BigInt *a = make_bigint("-5");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_not(result, a)
    );

    assert_bigint_string(result, "4");

    bigint_destroy(a);
    bigint_destroy(result);
}


/* ============================================================
   SHIFT LEFT
   ============================================================ */

void test_shift_left_zero(void)
{
    BigInt *a = make_bigint("123");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_shift_left(result, a, 0)
    );

    assert_bigint_string(result, "123");

    bigint_destroy(a);
    bigint_destroy(result);
}


void test_shift_left_basic(void)
{
    BigInt *a = make_bigint("5");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_shift_left(result, a, 3)
    );

    assert_bigint_string(result, "40");

    bigint_destroy(a);
    bigint_destroy(result);
}


void test_shift_left_large(void)
{
    BigInt *a = make_bigint("1");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_shift_left(result, a, 100)
    );

    /*
       2^100
       = 1267650600228229401496703205376
    */

    assert_bigint_string(
        result,
        "1267650600228229401496703205376"
    );

    bigint_destroy(a);
    bigint_destroy(result);
}


void test_shift_left_negative(void)
{
    BigInt *a = make_bigint("-5");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_shift_left(result, a, 3)
    );

    assert_bigint_string(result, "-40");

    bigint_destroy(a);
    bigint_destroy(result);
}


/* ============================================================
   SHIFT RIGHT
   ============================================================ */

void test_shift_right_zero(void)
{
    BigInt *a = make_bigint("123");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_shift_right(result, a, 0)
    );

    assert_bigint_string(result, "123");

    bigint_destroy(a);
    bigint_destroy(result);
}


void test_shift_right_basic(void)
{
    BigInt *a = make_bigint("40");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_shift_right(result, a, 3)
    );

    assert_bigint_string(result, "5");

    bigint_destroy(a);
    bigint_destroy(result);
}


void test_shift_right_truncation(void)
{
    BigInt *a = make_bigint("43");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_shift_right(result, a, 3)
    );

    assert_bigint_string(result, "5");

    bigint_destroy(a);
    bigint_destroy(result);
}


void test_shift_right_negative(void)
{
    BigInt *a = make_bigint("-43");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_shift_right(result, a, 3)
    );

    /* Truncating toward zero: -43 / 8 = -5 */

    assert_bigint_string(result, "-5");

    bigint_destroy(a);
    bigint_destroy(result);
}


void test_shift_right_too_far(void)
{
    BigInt *a = make_bigint("123");
    BigInt *result = bigint_create();

    TEST_ASSERT_EQUAL(
        BIGINT_OK,
        bigint_shift_right(result, a, 1000)
    );

    assert_bigint_string(result, "0");

    bigint_destroy(a);
    bigint_destroy(result);
}


/* ============================================================
   EVEN / ODD
   ============================================================ */

void test_is_even(void)
{
    BigInt *even = make_bigint("100");
    BigInt *odd = make_bigint("101");

    TEST_ASSERT_TRUE(bigint_is_even(even));
    TEST_ASSERT_FALSE(bigint_is_even(odd));

    bigint_destroy(even);
    bigint_destroy(odd);
}


void test_is_odd(void)
{
    BigInt *even = make_bigint("100");
    BigInt *odd = make_bigint("101");

    TEST_ASSERT_FALSE(bigint_is_odd(even));
    TEST_ASSERT_TRUE(bigint_is_odd(odd));

    bigint_destroy(even);
    bigint_destroy(odd);
}


void test_even_odd_zero(void)
{
    BigInt *zero = make_bigint("0");

    TEST_ASSERT_TRUE(bigint_is_even(zero));
    TEST_ASSERT_FALSE(bigint_is_odd(zero));

    bigint_destroy(zero);
}


void test_even_odd_negative(void)
{
    BigInt *even = make_bigint("-100");
    BigInt *odd = make_bigint("-101");

    TEST_ASSERT_TRUE(bigint_is_even(even));
    TEST_ASSERT_TRUE(bigint_is_odd(odd));

    bigint_destroy(even);
    bigint_destroy(odd);
}


/* ============================================================
   PROBABLE PRIME
   ============================================================ */

void test_prime_small_primes(void)
{
    BigInt *p2 = make_bigint("2");
    BigInt *p3 = make_bigint("3");
    BigInt *p17 = make_bigint("17");
    BigInt *p97 = make_bigint("97");

    TEST_ASSERT_TRUE(bigint_is_probable_prime(p2));
    TEST_ASSERT_TRUE(bigint_is_probable_prime(p3));
    TEST_ASSERT_TRUE(bigint_is_probable_prime(p17));
    TEST_ASSERT_TRUE(bigint_is_probable_prime(p97));

    bigint_destroy(p2);
    bigint_destroy(p3);
    bigint_destroy(p17);
    bigint_destroy(p97);
}


void test_prime_composites(void)
{
    BigInt *p0 = make_bigint("0");
    BigInt *p1 = make_bigint("1");
    BigInt *p4 = make_bigint("4");
    BigInt *p15 = make_bigint("15");
    BigInt *p100 = make_bigint("100");

    TEST_ASSERT_FALSE(bigint_is_probable_prime(p0));
    TEST_ASSERT_FALSE(bigint_is_probable_prime(p1));
    TEST_ASSERT_FALSE(bigint_is_probable_prime(p4));
    TEST_ASSERT_FALSE(bigint_is_probable_prime(p15));
    TEST_ASSERT_FALSE(bigint_is_probable_prime(p100));

    bigint_destroy(p0);
    bigint_destroy(p1);
    bigint_destroy(p4);
    bigint_destroy(p15);
    bigint_destroy(p100);
}


void test_prime_negative(void)
{
    BigInt *x = make_bigint("-17");

    TEST_ASSERT_FALSE(bigint_is_probable_prime(x));

    bigint_destroy(x);
}


/* ============================================================
   PERFECT SQUARE
   ============================================================ */

void test_perfect_square_zero(void)
{
    BigInt *x = make_bigint("0");

    TEST_ASSERT_TRUE(bigint_is_perfect_square(x));

    bigint_destroy(x);
}


void test_perfect_square_one(void)
{
    BigInt *x = make_bigint("1");

    TEST_ASSERT_TRUE(bigint_is_perfect_square(x));

    bigint_destroy(x);
}


void test_perfect_square_positive(void)
{
    BigInt *x = make_bigint("144");

    TEST_ASSERT_TRUE(bigint_is_perfect_square(x));

    bigint_destroy(x);
}


void test_perfect_square_large(void)
{
    BigInt *x = make_bigint(
        "15241578750190521"
    );

    /*
       123456789^2
       = 15241578750190521
    */

    TEST_ASSERT_TRUE(bigint_is_perfect_square(x));

    bigint_destroy(x);
}


void test_not_perfect_square(void)
{
    BigInt *x = make_bigint("145");

    TEST_ASSERT_FALSE(bigint_is_perfect_square(x));

    bigint_destroy(x);
}


void test_negative_not_perfect_square(void)
{
    BigInt *x = make_bigint("-144");

    TEST_ASSERT_FALSE(bigint_is_perfect_square(x));

    bigint_destroy(x);
}


/* ============================================================
   NULL argument tests
   ============================================================ */

void test_arithmetic_null_arguments(void)
{
    BigInt *a = make_bigint("10");
    BigInt *b = make_bigint("5");
    BigInt *r = bigint_create();

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
        bigint_div_mod(r, bigint_create(), a, NULL)
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
}


/* ============================================================
   Main
   ============================================================ */

int main(void)
{
    UNITY_BEGIN();

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

    /* Division / modulo */
    RUN_TEST(test_div_positive);
    RUN_TEST(test_div_negative);
    RUN_TEST(test_div_negative_divisor);
    RUN_TEST(test_division_by_zero);
    RUN_TEST(test_mod_positive);
    RUN_TEST(test_mod_negative);
    RUN_TEST(test_div_mod);
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
    RUN_TEST(test_pow_large_digits);
    RUN_TEST(test_pow_large);
    RUN_TEST(test_pow_negative_exponent);

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
    RUN_TEST(test_factorial_one);
    RUN_TEST(test_factorial_five);
    RUN_TEST(test_factorial_ten);
    RUN_TEST(test_factorial_negative);

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
    RUN_TEST(test_prime_negative);

    /* Perfect square */
    RUN_TEST(test_perfect_square_zero);
    RUN_TEST(test_perfect_square_one);
    RUN_TEST(test_perfect_square_positive);
    RUN_TEST(test_perfect_square_large);
    RUN_TEST(test_not_perfect_square);
    RUN_TEST(test_negative_not_perfect_square);

    /* NULL arguments */
    RUN_TEST(test_arithmetic_null_arguments);

    return UNITY_END();
}