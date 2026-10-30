#include "tests/support/numbers.h"
#include <numforge/bigrational.h>
#include "src/runtime/numforge_alloc.h"

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
    TEST_ASSERT_EQUAL(BIGRATIONAL_DIVISION_BY_ZERO, bigrational_div(value, zero, zero));
    TEST_ASSERT_EQUAL(BIGRATIONAL_DIVISION_BY_ZERO, bigrational_div(zero, zero, zero));
    test_assert_fraction("0", zero);
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
    for (long long da = 1; da <= 6; ++da) for (long long db = 1; db <= 6; ++db)
    for (long long numerator_a = -4; numerator_a <= 4; numerator_a++)
    {
        for (long long numerator_b = -4; numerator_b <= 4; numerator_b++)
        {
            BigRational *a = test_fraction("1", "2");
            BigRational *b = test_fraction("1", "3");
            BigRational *out = bigrational_create();
            BigInt *top_a = bigint_create(), *top_b = bigint_create();
            BigInt *denom_a = bigint_create(), *denom_b = bigint_create();
            char text[32];
            int comparison = 0;
            TEST_ASSERT_NOT_NULL(out);
            TEST_ASSERT_NOT_NULL(top_a);
            TEST_ASSERT_NOT_NULL(top_b);
            TEST_ASSERT_NOT_NULL(denom_a); TEST_ASSERT_NOT_NULL(denom_b);
            (void)snprintf(text, sizeof(text), "%lld", da);
            TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(denom_a, text));
            (void)snprintf(text, sizeof(text), "%lld", db);
            TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(denom_b, text));
            (void)snprintf(text, sizeof(text), "%lld", numerator_a);
            TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(top_a, text));
            (void)snprintf(text, sizeof(text), "%lld", numerator_b);
            TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(top_b, text));
            TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_set_fraction(a, top_a, denom_a));
            TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_set_fraction(b, top_b, denom_b));
            TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_add(out, a, b));
            assert_reference_fraction(db * numerator_a + da * numerator_b, da * db, out);
            TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_sub(out, a, b));
            assert_reference_fraction(db * numerator_a - da * numerator_b, da * db, out);
            TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_mul(out, a, b));
            assert_reference_fraction(numerator_a * numerator_b, da * db, out);
            if (numerator_b != 0)
            {
                TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_div(out, a, b));
                assert_reference_fraction(db * numerator_a, da * numerator_b, out);
            }
            TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_compare(&comparison, a, b));
            TEST_ASSERT_EQUAL_INT((db * numerator_a > da * numerator_b) - (db * numerator_a < da * numerator_b),
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

/* Exercise each allocation/checkpoint of the distinct canonical paths. Build
 * fixtures before injection and inspect outputs only after disabling it. */
static void test_optimized_paths_preserve_outputs_on_faults(void)
{
    typedef BigRationalStatus (*Operation)(BigRational *, const BigRational *, const BigRational *);
    const Operation operations[] = {bigrational_add, bigrational_sub, bigrational_mul, bigrational_div};
    const char *cases[][8] = {
        {"1","6","1","6","1/3","0","1/36","1"},
        {"0","1","-5","7","-5/7","5/7","0","0"},
        {"7","8","1","1","15/8","-1/8","7/8","7/8"},
        {"2","3","-9","10","-7/30","47/30","-3/5","-20/27"},
        {"1","6","1","10","4/15","1/15","1/60","5/3"},
        {"1","6","1","15","7/30","1/10","1/90","5/2"},
        {"-2","5","3","5","1/5","-1","-6/25","-2/3"},
        {"2","5","-3","5","-1/5","1","-6/25","-2/3"},
        {"-2","5","-3","5","-1","1/5","6/25","2/3"},
        {"-7","1","2","5","-33/5","-37/5","-14/5","-35/2"},
        {"2","5","-7","1","-33/5","37/5","-14/5","-2/35"},
        {"-7","1","-7","1","-14","0","49","1"}
    };
    for (size_t c = 0; c < sizeof(cases)/sizeof(cases[0]); ++c)
        for (size_t op = 0; op < 4; ++op) for (int alias = 0; alias < 3; ++alias)
            for (int deadline = 0; deadline < 2; ++deadline) {
                bool completed = false;
                for (size_t index = 1; index < 512; ++index) {
                    BigRational *a = test_fraction(cases[c][0], cases[c][1]);
                    BigRational *b = test_fraction(cases[c][2], cases[c][3]);
                    BigRational *separate = test_fraction("13", "17"), *out = alias == 1 ? a : alias == 2 ? b : separate;
                    char *original = NULL;
                    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_to_string(out, &original));
                    bool owner = numforge_budget_begin(60000, SIZE_MAX, SIZE_MAX);
                    TEST_ASSERT_TRUE(owner);
                    numforge_test_allocator_begin(deadline ? 0 : index);
                    if (deadline) numforge_test_budget_expire_after(index);
                    BigRationalStatus status = operations[op](out, a, b);
                    bool failed = deadline ? numforge_budget_failure() == NUMFORGE_BUDGET_TIME : numforge_test_allocator_did_fail();
                    numforge_budget_end(); numforge_test_allocator_end();
                    if (failed) {
                        TEST_ASSERT_EQUAL(BIGRATIONAL_OUT_OF_MEMORY, status);
                        test_assert_fraction(original, out);
                    } else {
                        TEST_ASSERT_EQUAL(BIGRATIONAL_OK, status);
                        test_assert_fraction(cases[c][4+op], out);
                        completed = true;
                    }
                    free(original); bigrational_destroy(a); bigrational_destroy(b); bigrational_destroy(separate);
                    if (completed) break;
                }
                TEST_ASSERT_TRUE(completed);
            }
}

static void test_large_self_products_preserve_outputs_on_faults(void)
{
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_TRUE(numforge_alloc_stats_track(true));
#endif
    const size_t bits[] = {1024U, 8192U};
    for (size_t b = 0U; b < sizeof(bits)/sizeof(bits[0]); ++b) {
        BigInt *one = test_integer("1"), *n = test_integer("1"), *d = test_integer("1");
        BigInt *square_n = test_integer("1"), *square_d = test_integer("1"), *correction = test_integer("1");
        BigRational *input = bigrational_create(), *square = bigrational_create();
        char *input_text = NULL, *square_text = NULL;
        TEST_ASSERT_NOT_NULL(input); TEST_ASSERT_NOT_NULL(square);
        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_shift_left(d, d, bits[b]));
        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_sub(n, d, one));
        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_negate(n, n));
        TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_set_fraction(input, n, d));
        /* Independent square identity: (2^k-1)^2=2^(2k)-2^(k+1)+1. */
        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_shift_left(square_d, square_d, 2U*bits[b]));
        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_shift_left(correction, correction, bits[b]+1U));
        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_sub(square_n, square_d, correction));
        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_add(square_n, square_n, one));
        TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_set_fraction(square, square_n, square_d));
        TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_to_string(input, &input_text));
        TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_to_string(square, &square_text));
        for (unsigned divide = 0U; divide < 2U; ++divide) for (unsigned alias = 0U; alias < 2U; ++alias)
        for (unsigned deadline = 0U; deadline < 2U; ++deadline) {
            bool completed = false;
            for (size_t index = 1U; index < 4096U; ++index) {
                BigRational *a = bigrational_create(), *separate = test_fraction("13", "17");
                TEST_ASSERT_NOT_NULL(a);
                TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_copy(a, input));
                BigRational *out = alias ? a : separate;
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
                size_t baseline = numforge_alloc_stats_live();
#endif
                TEST_ASSERT_TRUE(numforge_budget_begin(60000U, SIZE_MAX, SIZE_MAX));
                numforge_test_allocator_begin(deadline ? 0U : index);
                if (deadline) numforge_test_budget_expire_after(index);
                BigRationalStatus status = divide ? bigrational_div(out, a, a) : bigrational_mul(out, a, a);
                bool failed = deadline ? numforge_budget_failure() == NUMFORGE_BUDGET_TIME : numforge_test_allocator_did_fail();
                numforge_budget_end(); numforge_test_allocator_end();
                if (failed) {
                    TEST_ASSERT_EQUAL(BIGRATIONAL_OUT_OF_MEMORY, status);
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
                    TEST_ASSERT_EQUAL_UINT64(baseline, numforge_alloc_stats_live());
#endif
                    test_assert_fraction(input_text, a); test_assert_fraction("13/17", separate);
                } else {
                    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, status);
                    test_assert_fraction(divide ? "1" : square_text, out);
                    if (!alias) test_assert_fraction(input_text, a);
                    completed = true;
                }
                bigrational_destroy(a); bigrational_destroy(separate);
                if (completed) break;
            }
            TEST_ASSERT_TRUE(completed);
        }
        free(input_text); free(square_text); bigrational_destroy(input); bigrational_destroy(square);
        bigint_destroy(one); bigint_destroy(n); bigint_destroy(d); bigint_destroy(square_n);
        bigint_destroy(square_d); bigint_destroy(correction);
    }
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_EQUAL_UINT64(0U, numforge_alloc_stats_live());
    TEST_ASSERT_TRUE(numforge_alloc_stats_complete());
    TEST_ASSERT_TRUE(numforge_alloc_stats_track(false));
#endif
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_canonical_lifetime_and_parts);
    RUN_TEST(test_arithmetic_comparison_and_aliases);
    RUN_TEST(test_decimal_conversion_and_rounding);
    RUN_TEST(test_domains_preserve_outputs);
    RUN_TEST(test_small_integer_reference_grid);
    RUN_TEST(test_optimized_paths_preserve_outputs_on_faults);
    RUN_TEST(test_large_self_products_preserve_outputs_on_faults);
    return UNITY_END();
}
