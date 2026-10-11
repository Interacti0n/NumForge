#include <stdlib.h>
#include <string.h>

#include <unity.h>

#include <numforge/bigdecimal.h>
#include <numforge/bigint.h>
#include <numforge/bigrational.h>

#include "numforge_alloc.h"
#include "evaluator.h"
#include "formatter.h"
#include "parser.h"
#include "calculator_internal.h"
#include "web_api.h"
#include "../src/bigint/bigint_internal.h"
#include "../src/bigdecimal/bigdecimal_internal.h"

#ifndef NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING
#error "allocation_failure_tests requires NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING"
#endif

#define ALLOCATION_TEST_MAX_FAILURE_INDEX 4096U

typedef BigIntStatus (*BigIntUnaryOperation)(BigInt *, const BigInt *);
typedef BigIntStatus (*BigIntBinaryOperation)(BigInt *, const BigInt *, const BigInt *);
typedef BigDecimalStatus (*BigDecimalUnaryOperation)(BigDecimal *, const BigDecimal *);
typedef BigDecimalStatus (*BigDecimalBinaryOperation)(
    BigDecimal *,
    const BigDecimal *,
    const BigDecimal *
);

void setUp(void)
{
    numforge_budget_end();
    numforge_test_allocator_end();
}

void tearDown(void)
{
    numforge_budget_end();
    numforge_test_allocator_end();
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Test value and assertion helpers.
------------------------------------------------------------------------------------------------------------------------------
*/

static BigInt *make_bigint(const char *text)
{
    BigInt *value = bigint_create();

    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(value, text));
    return value;
}

static BigDecimal *make_bigdecimal(const char *text)
{
    BigDecimal *value = bigdecimal_create();

    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, text));
    return value;
}

static void assert_bigint_text(const char *expected, const BigInt *value)
{
    char *actual = bigint_to_string(value);

    TEST_ASSERT_NOT_NULL(actual);
    TEST_ASSERT_EQUAL_STRING(expected, actual);
    free(actual);
}

static void assert_bigdecimal_text(const char *expected, const BigDecimal *value)
{
    char *actual = NULL;

    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_to_string(value, &actual));
    TEST_ASSERT_NOT_NULL(actual);
    TEST_ASSERT_EQUAL_STRING(expected, actual);
    free(actual);
}

static BigRational *make_bigrational(const char *numerator, const char *denominator)
{
    BigInt *top = make_bigint(numerator);
    BigInt *bottom = make_bigint(denominator);
    BigRational *value = bigrational_create();
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_set_fraction(value, top, bottom));
    bigint_destroy(top);
    bigint_destroy(bottom);
    return value;
}

static void assert_bigrational_text(const char *expected, const BigRational *value)
{
    char *actual = NULL;
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_to_string(value, &actual));
    TEST_ASSERT_EQUAL_STRING(expected, actual);
    free(actual);
}

static BigIntStatus bigint_shift_left_129(BigInt *result, const BigInt *value)
{
    return bigint_shift_left(result, value, 129U);
}

static BigIntStatus bigint_shift_right_65(BigInt *result, const BigInt *value)
{
    return bigint_shift_right(result, value, 65U);
}

static BigDecimalStatus bigdecimal_rescale_to_25(BigDecimal *result, const BigDecimal *value)
{
    return bigdecimal_rescale(result, value, 25, BIGDECIMAL_ROUND_HALF_EVEN);
}

static BigDecimalStatus bigdecimal_round_to_minus_5(BigDecimal *result, const BigDecimal *value)
{
    return bigdecimal_round(result, value, -5);
}

static BigDecimalStatus bigdecimal_exp_to_25(BigDecimal *result, const BigDecimal *value)
{
    return bigdecimal_exp(result, value, 25, BIGDECIMAL_ROUND_HALF_EVEN);
}

static BigDecimalStatus bigdecimal_ln_to_25(BigDecimal *result, const BigDecimal *value)
{
    return bigdecimal_ln(result, value, 25, BIGDECIMAL_ROUND_HALF_EVEN);
}

static BigDecimalStatus bigdecimal_pi_to_520(BigDecimal *result, const BigDecimal *value)
{
    (void)value;
    return bigdecimal_set_constant_significant(
        result, BIGDECIMAL_CONSTANT_PI, 520, BIGDECIMAL_ROUND_HALF_EVEN);
}

static BigDecimalStatus bigdecimal_sin_to_25(BigDecimal *result, const BigDecimal *value)
{
    return bigdecimal_sin(result, value, 25, BIGDECIMAL_ROUND_HALF_EVEN);
}

static BigDecimalStatus bigdecimal_atan_to_25(BigDecimal *result, const BigDecimal *value)
{
    return bigdecimal_atan(result, value, 25, BIGDECIMAL_ROUND_HALF_EVEN);
}

static BigDecimalStatus bigdecimal_sinh_to_25(BigDecimal *result, const BigDecimal *value)
{
    return bigdecimal_sinh(result, value, 25, BIGDECIMAL_ROUND_HALF_EVEN);
}

static BigDecimalStatus bigdecimal_cosh_to_25(BigDecimal *result, const BigDecimal *value)
{
    return bigdecimal_cosh(result, value, 25, BIGDECIMAL_ROUND_HALF_EVEN);
}

static BigDecimalStatus bigdecimal_tanh_to_25(BigDecimal *result, const BigDecimal *value)
{
    return bigdecimal_tanh(result, value, 25, BIGDECIMAL_ROUND_HALF_EVEN);
}

static BigDecimalStatus bigdecimal_asinh_to_25(BigDecimal *result, const BigDecimal *value)
{
    return bigdecimal_asinh(result, value, 25, BIGDECIMAL_ROUND_HALF_EVEN);
}

static BigDecimalStatus bigdecimal_acosh_to_25(BigDecimal *result, const BigDecimal *value)
{
    return bigdecimal_acosh(result, value, 25, BIGDECIMAL_ROUND_HALF_EVEN);
}

static BigDecimalStatus bigdecimal_atanh_to_25(BigDecimal *result, const BigDecimal *value)
{
    return bigdecimal_atanh(result, value, 25, BIGDECIMAL_ROUND_HALF_EVEN);
}

static BigDecimalStatus bigdecimal_power_minus_three(
    BigDecimal *result,
    const BigDecimal *value
)
{
    BigInt *exponent = bigint_create();
    BigIntStatus integer_status;
    BigDecimalStatus status;

    if (exponent == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    integer_status = bigint_set_string(exponent, "-3");

    if (integer_status != BIGINT_OK)
    {
        bigint_destroy(exponent);
        return integer_status == BIGINT_OUT_OF_MEMORY
                   ? BIGDECIMAL_OUT_OF_MEMORY
                   : BIGDECIMAL_INVALID_ARGUMENT;
    }

    status = bigdecimal_pow_signed(
        result, value, exponent, 25, BIGDECIMAL_ROUND_HALF_EVEN);
    bigint_destroy(exponent);
    return status;
}

static BigDecimalStatus bigdecimal_divide_to_25(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b
)
{
    return bigdecimal_div(result, a, b, 25, BIGDECIMAL_ROUND_HALF_EVEN);
}

static BigDecimalStatus bigdecimal_log_to_25(
    BigDecimal *result,
    const BigDecimal *value,
    const BigDecimal *base
)
{
    return bigdecimal_log(result, value, base, 25, BIGDECIMAL_ROUND_HALF_EVEN);
}

static BigDecimalStatus bigdecimal_sum_two(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b
)
{
    const BigDecimal *values[] = {a, b};

    return bigdecimal_sum(result, values, 2U);
}

static BigDecimalStatus bigdecimal_product_two(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b
)
{
    const BigDecimal *values[] = {a, b};

    return bigdecimal_product(result, values, 2U);
}

static BigDecimalStatus bigdecimal_mean_two(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b
)
{
    const BigDecimal *values[] = {a, b};

    return bigdecimal_mean(
        result, values, 2U, 25, BIGDECIMAL_ROUND_HALF_EVEN);
}

static BigDecimalStatus bigdecimal_median_two(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b
)
{
    const BigDecimal *values[] = {a, b};

    return bigdecimal_median(result, values, 2U);
}

static BigDecimalStatus bigdecimal_geomean_two(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b
)
{
    const BigDecimal *values[] = {a, b};

    return bigdecimal_geometric_mean(
        result, values, 2U, 25, BIGDECIMAL_ROUND_HALF_EVEN);
}

static BigDecimalStatus bigdecimal_harmean_two(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b
)
{
    const BigDecimal *values[] = {a, b};

    return bigdecimal_harmonic_mean(
        result, values, 2U, 25, BIGDECIMAL_ROUND_HALF_EVEN);
}

static BigDecimalStatus bigdecimal_variance_two(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b
)
{
    const BigDecimal *values[] = {a, b};

    return bigdecimal_variance_population(
        result, values, 2U, 25, BIGDECIMAL_ROUND_HALF_EVEN);
}

static BigDecimalStatus bigdecimal_stdevp_two(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b
)
{
    const BigDecimal *values[] = {a, b};

    return bigdecimal_standard_deviation_population(
        result, values, 2U, 25, BIGDECIMAL_ROUND_HALF_EVEN);
}

static BigDecimalStatus bigdecimal_stdev_two(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b
)
{
    const BigDecimal *values[] = {a, b};

    return bigdecimal_standard_deviation_sample(
        result, values, 2U, 25, BIGDECIMAL_ROUND_HALF_EVEN);
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Exhaust one operation's allocation sites. Every injected failure must
    report out-of-memory and preserve the caller-owned destination.
------------------------------------------------------------------------------------------------------------------------------
*/

static void assert_bigint_unary_failure_safety(
    BigIntUnaryOperation operation,
    const char *input_text
)
{
    bool completed = false;

    for (size_t failure_index = 1U;
         failure_index <= ALLOCATION_TEST_MAX_FAILURE_INDEX;
         failure_index++)
    {
        BigInt *result = make_bigint("777");
        BigInt *input = make_bigint(input_text);
        BigIntStatus status;
        bool injected;
        size_t call_count;

        numforge_test_allocator_begin(failure_index);
        status = operation(result, input);
        injected = numforge_test_allocator_did_fail();
        call_count = numforge_test_allocator_call_count();
        numforge_test_allocator_end();

        if (injected)
        {
            TEST_ASSERT_EQUAL(BIGINT_OUT_OF_MEMORY, status);
            assert_bigint_text("777", result);
        }
        else
        {
            TEST_ASSERT_EQUAL(BIGINT_OK, status);
            TEST_ASSERT_TRUE(call_count < failure_index);
            completed = true;
        }

        bigint_destroy(result);
        bigint_destroy(input);
        if (completed) break;
    }

    TEST_ASSERT_TRUE(completed);
}

static void assert_bigint_binary_failure_safety(
    BigIntBinaryOperation operation,
    const char *a_text,
    const char *b_text
)
{
    bool completed = false;

    for (size_t failure_index = 1U;
         failure_index <= ALLOCATION_TEST_MAX_FAILURE_INDEX;
         failure_index++)
    {
        BigInt *result = make_bigint("777");
        BigInt *a = make_bigint(a_text);
        BigInt *b = make_bigint(b_text);
        BigIntStatus status;
        bool injected;

        numforge_test_allocator_begin(failure_index);
        status = operation(result, a, b);
        injected = numforge_test_allocator_did_fail();
        numforge_test_allocator_end();
        assert_bigint_text(a_text, a);
        assert_bigint_text(b_text, b);

        if (injected)
        {
            TEST_ASSERT_EQUAL(BIGINT_OUT_OF_MEMORY, status);
            assert_bigint_text("777", result);
        }
        else
        {
            TEST_ASSERT_EQUAL(BIGINT_OK, status);
            completed = true;
        }

        bigint_destroy(result);
        bigint_destroy(a);
        bigint_destroy(b);
        if (completed) break;
    }

    TEST_ASSERT_TRUE(completed);
}

static void assert_bigint_binary_alias_failure_safety(
    BigIntBinaryOperation operation,
    const char *a_text,
    const char *b_text
)
{
    bool completed = false;

    for (size_t failure_index = 1U;
         failure_index <= ALLOCATION_TEST_MAX_FAILURE_INDEX;
         failure_index++)
    {
        BigInt *a = make_bigint(a_text);
        BigInt *b = make_bigint(b_text);
        BigIntStatus status;
        bool injected;

        numforge_test_allocator_begin(failure_index);
        status = operation(a, a, b);
        injected = numforge_test_allocator_did_fail();
        numforge_test_allocator_end();

        if (injected)
        {
            TEST_ASSERT_EQUAL(BIGINT_OUT_OF_MEMORY, status);
            assert_bigint_text(a_text, a);
        }
        else
        {
            TEST_ASSERT_EQUAL(BIGINT_OK, status);
            completed = true;
        }

        bigint_destroy(a);
        bigint_destroy(b);
        if (completed) break;
    }

    TEST_ASSERT_TRUE(completed);
}

static void assert_bigdecimal_unary_failure_safety(
    BigDecimalUnaryOperation operation,
    const char *input_text
)
{
    bool completed = false;

    for (size_t failure_index = 1U;
         failure_index <= ALLOCATION_TEST_MAX_FAILURE_INDEX;
         failure_index++)
    {
        BigDecimal *result = make_bigdecimal("7.77");
        BigDecimal *input = make_bigdecimal(input_text);
        BigDecimalStatus status;
        bool injected;

        numforge_test_allocator_begin(failure_index);
        status = operation(result, input);
        injected = numforge_test_allocator_did_fail();
        numforge_test_allocator_end();

        if (injected)
        {
            TEST_ASSERT_EQUAL(BIGDECIMAL_OUT_OF_MEMORY, status);
            assert_bigdecimal_text("7.77", result);
        }
        else
        {
            TEST_ASSERT_EQUAL(BIGDECIMAL_OK, status);
            completed = true;
        }

        bigdecimal_destroy(result);
        bigdecimal_destroy(input);
        if (completed) break;
    }

    TEST_ASSERT_TRUE(completed);
}

static void assert_bigdecimal_binary_failure_safety(
    BigDecimalBinaryOperation operation,
    const char *a_text,
    const char *b_text
)
{
    bool completed = false;

    for (size_t failure_index = 1U;
         failure_index <= ALLOCATION_TEST_MAX_FAILURE_INDEX;
         failure_index++)
    {
        BigDecimal *result = make_bigdecimal("7.77");
        BigDecimal *a = make_bigdecimal(a_text);
        BigDecimal *b = make_bigdecimal(b_text);
        BigDecimalStatus status;
        bool injected;

        numforge_test_allocator_begin(failure_index);
        status = operation(result, a, b);
        injected = numforge_test_allocator_did_fail();
        numforge_test_allocator_end();

        if (injected)
        {
            TEST_ASSERT_EQUAL(BIGDECIMAL_OUT_OF_MEMORY, status);
            assert_bigdecimal_text("7.77", result);
        }
        else
        {
            TEST_ASSERT_EQUAL(BIGDECIMAL_OK, status);
            completed = true;
        }

        bigdecimal_destroy(result);
        bigdecimal_destroy(a);
        bigdecimal_destroy(b);
        if (completed) break;
    }

    TEST_ASSERT_TRUE(completed);
}

static void assert_bigdecimal_binary_alias_failure_safety(
    BigDecimalBinaryOperation operation,
    const char *a_text,
    const char *b_text
)
{
    bool completed = false;

    for (size_t failure_index = 1U;
         failure_index <= ALLOCATION_TEST_MAX_FAILURE_INDEX;
         failure_index++)
    {
        BigDecimal *a = make_bigdecimal(a_text);
        BigDecimal *b = make_bigdecimal(b_text);
        BigDecimalStatus status;
        bool injected;

        numforge_test_allocator_begin(failure_index);
        status = operation(a, a, b);
        injected = numforge_test_allocator_did_fail();
        numforge_test_allocator_end();

        if (injected)
        {
            TEST_ASSERT_EQUAL(BIGDECIMAL_OUT_OF_MEMORY, status);
            assert_bigdecimal_text(a_text, a);
        }
        else
        {
            TEST_ASSERT_EQUAL(BIGDECIMAL_OK, status);
            completed = true;
        }

        bigdecimal_destroy(a);
        bigdecimal_destroy(b);
        if (completed) break;
    }

    TEST_ASSERT_TRUE(completed);
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Allocation controller and lifetime tests.
------------------------------------------------------------------------------------------------------------------------------
*/

void test_allocator_injects_malloc_calloc_and_realloc_failures(void)
{
    void *memory = malloc(8U);

    TEST_ASSERT_NOT_NULL(memory);

    numforge_test_allocator_begin(1U);
    TEST_ASSERT_NULL(numforge_malloc(8U));
    TEST_ASSERT_TRUE(numforge_test_allocator_did_fail());
    TEST_ASSERT_EQUAL_UINT64(1U, numforge_test_allocator_call_count());
    numforge_test_allocator_end();

    numforge_test_allocator_begin(1U);
    TEST_ASSERT_NULL(numforge_calloc(2U, 8U));
    TEST_ASSERT_TRUE(numforge_test_allocator_did_fail());
    numforge_test_allocator_end();

    numforge_test_allocator_begin(1U);
    TEST_ASSERT_NULL(numforge_realloc(memory, 16U));
    TEST_ASSERT_TRUE(numforge_test_allocator_did_fail());
    numforge_test_allocator_end();

    free(memory);
}

void test_numeric_creation_cleans_up_every_failed_allocation(void)
{
    bool bigint_created = false;
    bool bigdecimal_created = false;

    for (size_t failure_index = 1U; failure_index <= 4U; failure_index++)
    {
        BigInt *value;
        bool injected;

        numforge_test_allocator_begin(failure_index);
        value = bigint_create();
        injected = numforge_test_allocator_did_fail();
        numforge_test_allocator_end();

        if (injected) TEST_ASSERT_NULL(value);
        else
        {
            TEST_ASSERT_NOT_NULL(value);
            bigint_created = true;
            bigint_destroy(value);
            break;
        }
    }

    for (size_t failure_index = 1U; failure_index <= 5U; failure_index++)
    {
        BigDecimal *value;
        bool injected;

        numforge_test_allocator_begin(failure_index);
        value = bigdecimal_create();
        injected = numforge_test_allocator_did_fail();
        numforge_test_allocator_end();

        if (injected) TEST_ASSERT_NULL(value);
        else
        {
            TEST_ASSERT_NOT_NULL(value);
            bigdecimal_created = true;
            bigdecimal_destroy(value);
            break;
        }
    }

    TEST_ASSERT_TRUE(bigint_created);
    TEST_ASSERT_TRUE(bigdecimal_created);
}

/*
------------------------------------------------------------------------------------------------------------------------------
    BigInt failure guarantees.
------------------------------------------------------------------------------------------------------------------------------
*/

void test_bigint_conversion_failure_paths(void)
{
    static const char large_value[] =
        "1234567890123456789012345678901234567890123456789012345678901234567890";
    bool parse_completed = false;
    bool format_completed = false;

    for (size_t failure_index = 1U;
         failure_index <= ALLOCATION_TEST_MAX_FAILURE_INDEX;
         failure_index++)
    {
        BigInt *value = make_bigint("777");
        BigIntStatus status;
        bool injected;

        numforge_test_allocator_begin(failure_index);
        status = bigint_set_string(value, large_value);
        injected = numforge_test_allocator_did_fail();
        numforge_test_allocator_end();

        if (injected)
        {
            TEST_ASSERT_EQUAL(BIGINT_OUT_OF_MEMORY, status);
            assert_bigint_text("777", value);
        }
        else
        {
            TEST_ASSERT_EQUAL(BIGINT_OK, status);
            parse_completed = true;
        }
        bigint_destroy(value);
        if (parse_completed) break;
    }

    for (size_t failure_index = 1U;
         failure_index <= ALLOCATION_TEST_MAX_FAILURE_INDEX;
         failure_index++)
    {
        BigInt *value = make_bigint(large_value);
        char *text;
        bool injected;

        numforge_test_allocator_begin(failure_index);
        text = bigint_to_string(value);
        injected = numforge_test_allocator_did_fail();
        numforge_test_allocator_end();

        if (injected) TEST_ASSERT_NULL(text);
        else
        {
            TEST_ASSERT_EQUAL_STRING(large_value, text);
            format_completed = true;
        }
        free(text);
        bigint_destroy(value);
        if (format_completed) break;
    }

    TEST_ASSERT_TRUE(parse_completed);
    TEST_ASSERT_TRUE(format_completed);
}

void test_bigint_arithmetic_failure_paths(void)
{
    static const char a[] = "12345678901234567890123456789012345678901234567890";
    static const char b[] = "987654321098765432109876543210987654321";

    assert_bigint_unary_failure_safety(bigint_copy, a);
    assert_bigint_unary_failure_safety(bigint_abs, "-123456789012345678901234567890");
    assert_bigint_unary_failure_safety(bigint_negate, a);
    assert_bigint_binary_failure_safety(bigint_add, a, b);
    assert_bigint_binary_failure_safety(bigint_sub, a, b);
    assert_bigint_binary_failure_safety(bigint_mul, a, b);
    assert_bigint_binary_failure_safety(bigint_div, a, b);
    assert_bigint_binary_failure_safety(bigint_mod, a, b);
    assert_bigint_binary_failure_safety(bigint_pow, "12345678901234567890", "13");
}

void test_limb_division_every_failure_and_alias(void)
{
    static const struct { const char *a; const char *b; const char *q; const char *r; } cases[] = {
        { "0", "97", "0", "0" },
        { "10", "97", "0", "10" },
        { "97", "97", "1", "0" },
        { "6277101735386680763835789423207666416102355444464034512895", "97",
          "64712389024604956328204014672243983671158303551175613535", "0" },
        { "340282366920938463500268095579187314685", "170141183460469231750134047789593657343",
          "1", "170141183460469231750134047789593657342" },
        { "36893488147419103233", "18446744073709551617", "1", "18446744073709551616" },
        { "6277101735386680763835789423207666416102355444464034512897",
          "3138550867693340381917894711603833208051177722232017256449",
          "1", "3138550867693340381917894711603833208051177722232017256448" }
    };
    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++)
    for (unsigned alias = 0; alias < 7U; alias++)
    for (unsigned deadline = 0; deadline < 2U; deadline++)
    {
        bool completed = false;
        for (size_t index = 1; index <= ALLOCATION_TEST_MAX_FAILURE_INDEX; index++)
        {
            BigInt *a = make_bigint(cases[c].a), *b = make_bigint(cases[c].b);
            BigInt *original_q = make_bigint("111"), *original_r = make_bigint("222");
            BigInt *q = alias == 1 || alias == 5 ? a : alias == 2 || alias == 6 ? b : original_q;
            BigInt *r = alias == 3 || alias == 6 ? a : alias == 4 || alias == 5 ? b : original_r;
            if (deadline)
            {
                (void)numforge_budget_begin(5000U, SIZE_MAX, SIZE_MAX);
                numforge_test_budget_expire_after(index);
            }
            else numforge_test_allocator_begin(index);
            BigIntStatus status = bigint_div_mod(q, r, a, b);
            bool interrupted = deadline ? numforge_budget_failure() != NUMFORGE_BUDGET_OK
                : numforge_test_allocator_did_fail();
            numforge_budget_end();
            numforge_test_allocator_end();
            if (status == BIGINT_OK)
            {
                TEST_ASSERT_FALSE(interrupted);
                assert_bigint_text(cases[c].q, q);
                assert_bigint_text(cases[c].r, r);
                if (a != q && a != r) assert_bigint_text(cases[c].a, a);
                if (b != q && b != r) assert_bigint_text(cases[c].b, b);
                completed = true;
            }
            else
            {
                TEST_ASSERT_TRUE(interrupted);
                TEST_ASSERT_EQUAL(BIGINT_OUT_OF_MEMORY, status);
                assert_bigint_text(cases[c].a, a);
                assert_bigint_text(cases[c].b, b);
                assert_bigint_text("111", original_q);
                assert_bigint_text("222", original_r);
            }
            bigint_destroy(a); bigint_destroy(b);
            bigint_destroy(original_q); bigint_destroy(original_r);
            if (completed) break;
        }
        TEST_ASSERT_TRUE(completed);
    }
}

void test_gcd_workspace_every_failure_and_alias(void)
{
    static const struct { const char *a; const char *b; const char *g; } cases[] = {
        { "0", "0", "0" }, { "0", "-97", "97" }, { "-97", "0", "97" },
        { "-97", "-97", "97" }, { "10", "97", "1" },
        { "-6277101735386680763835789423207666416102355444464034512895", "97", "97" },
        /* Consecutive Fibonacci numbers force repeated multi-limb remainders. */
        { "-354224848179261915075", "573147844013817084101", "1" }
    };
    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++)
    for (unsigned alias = 0; alias < 4U; alias++)
    for (unsigned deadline = 0; deadline < 2U; deadline++)
    {
        bool completed = false;
        for (size_t index = 1; index <= ALLOCATION_TEST_MAX_FAILURE_INDEX; index++)
        {
            BigInt *a = make_bigint(cases[c].a), *b = make_bigint(cases[c].b);
            BigInt *original = make_bigint("111");
            BigInt *result = alias == 1 || alias == 3 ? a : alias == 2 ? b : original;
            const BigInt *right = alias == 3 ? a : b;
            if (deadline)
            {
                (void)numforge_budget_begin(5000U, SIZE_MAX, SIZE_MAX);
                numforge_test_budget_expire_after(index);
            }
            else numforge_test_allocator_begin(index);
            BigIntStatus status = bigint_gcd(result, a, right);
            bool interrupted = deadline ? numforge_budget_failure() != NUMFORGE_BUDGET_OK
                : numforge_test_allocator_did_fail();
            numforge_budget_end(); numforge_test_allocator_end();
            if (status == BIGINT_OK)
            {
                TEST_ASSERT_FALSE(interrupted);
                const char *expected = alias == 3 ? cases[c].a : cases[c].g;
                if (*expected == '-') expected++;
                assert_bigint_text(expected, result);
                if (result != a) assert_bigint_text(cases[c].a, a);
                if (result != b) assert_bigint_text(cases[c].b, b);
                completed = true;
            }
            else
            {
                TEST_ASSERT_TRUE(interrupted);
                TEST_ASSERT_EQUAL(BIGINT_OUT_OF_MEMORY, status);
                assert_bigint_text(cases[c].a, a);
                assert_bigint_text(cases[c].b, b);
                assert_bigint_text("111", original);
            }
            bigint_destroy(a); bigint_destroy(b); bigint_destroy(original);
            if (completed) break;
        }
        TEST_ASSERT_TRUE(completed);
    }
}

void test_gcd_large_pair_fits_production_allocation_budget(void)
{
    BigInt *a = make_bigint("2"), *b = make_bigint("3"), *one = make_bigint("1");
    BigInt *power = make_bigint("100000"), *result = make_bigint("111");
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_pow(a, a, power));
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_sub(a, a, one));
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(power, "63000"));
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_pow(b, b, power));
    /* Known exact gcd is 3. This previously requested over 1 GiB cumulatively.
     * Enforce production memory caps without a machine-dependent timing test. */
    (void)numforge_budget_begin(UINT64_MAX, CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    BigIntStatus status = bigint_gcd(result, a, b);
    NumForgeBudgetFailure failure = numforge_budget_failure();
    numforge_budget_end();
    TEST_ASSERT_EQUAL(BIGINT_OK, status);
    TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_OK, failure);
    assert_bigint_text("3", result);
    bigint_destroy(a); bigint_destroy(b); bigint_destroy(one);
    bigint_destroy(power); bigint_destroy(result);
}

void test_bigint_divmod_and_number_theory_failure_paths(void)
{
    static const char a_text[] = "12345678901234567890123456789012345678901234567890";
    static const char b_text[] = "98765432109876543210987654321";
    bool completed = false;

    for (size_t failure_index = 1U;
         failure_index <= ALLOCATION_TEST_MAX_FAILURE_INDEX;
         failure_index++)
    {
        BigInt *quotient = make_bigint("111");
        BigInt *remainder = make_bigint("222");
        BigInt *a = make_bigint(a_text);
        BigInt *b = make_bigint(b_text);
        BigIntStatus status;
        bool injected;

        numforge_test_allocator_begin(failure_index);
        status = bigint_div_mod(quotient, remainder, a, b);
        injected = numforge_test_allocator_did_fail();
        numforge_test_allocator_end();

        if (injected)
        {
            TEST_ASSERT_EQUAL(BIGINT_OUT_OF_MEMORY, status);
            assert_bigint_text("111", quotient);
            assert_bigint_text("222", remainder);
        }
        else
        {
            TEST_ASSERT_EQUAL(BIGINT_OK, status);
            completed = true;
        }

        bigint_destroy(quotient);
        bigint_destroy(remainder);
        bigint_destroy(a);
        bigint_destroy(b);
        if (completed) break;
    }

    TEST_ASSERT_TRUE(completed);
    assert_bigint_binary_failure_safety(bigint_gcd, a_text, b_text);
    assert_bigint_binary_failure_safety(bigint_lcm, a_text, b_text);
    assert_bigint_binary_failure_safety(bigint_permutation, "100", "30");
    assert_bigint_binary_failure_safety(bigint_combination, "100", "30");
    assert_bigint_binary_failure_safety(bigint_permutation, "18446744073709551617", "3");
    assert_bigint_binary_failure_safety(bigint_combination, "18446744073709551617", "3");
    assert_bigint_unary_failure_safety(bigint_factorial, "50");
}

void test_bigint_bitwise_and_shift_failure_paths(void)
{
    static const char a[] = "340282366920938463463374607431768211455";
    static const char b[] = "226854911280625642308916404954512140970";

    assert_bigint_binary_failure_safety(bigint_and, a, b);
    assert_bigint_binary_failure_safety(bigint_or, a, b);
    assert_bigint_binary_failure_safety(bigint_xor, a, b);
    assert_bigint_unary_failure_safety(bigint_not, a);
    assert_bigint_unary_failure_safety(bigint_shift_left_129, a);
    assert_bigint_unary_failure_safety(bigint_shift_right_65, a);
}

void test_bigint_aliasing_preserves_destination_on_allocation_failure(void)
{
    static const char a[] = "12345678901234567890123456789012345678901234567890";
    static const char b[] = "98765432109876543210987654321";

    assert_bigint_binary_alias_failure_safety(bigint_add, a, b);
    assert_bigint_binary_alias_failure_safety(bigint_mul, a, b);
    assert_bigint_binary_alias_failure_safety(bigint_div, a, b);
    assert_bigint_binary_alias_failure_safety(bigint_pow, "12345678901234567890", "13");
}

void test_bigint_boolean_number_theory_handles_every_allocation_failure(void)
{
    BigInt *prime = make_bigint("97");
    BigInt *square = make_bigint("15241578750190521");
    size_t prime_allocations;
    size_t square_allocations;
    bool result = false;

    numforge_test_allocator_begin(0U);
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_is_probable_prime(&result, prime));
    TEST_ASSERT_TRUE(result);
    prime_allocations = numforge_test_allocator_call_count();
    numforge_test_allocator_end();

    result = false;
    numforge_test_allocator_begin(0U);
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_is_perfect_square(&result, square));
    TEST_ASSERT_TRUE(result);
    square_allocations = numforge_test_allocator_call_count();
    numforge_test_allocator_end();

    TEST_ASSERT_GREATER_THAN_UINT64(0U, prime_allocations);
    TEST_ASSERT_GREATER_THAN_UINT64(0U, square_allocations);
    TEST_ASSERT_LESS_OR_EQUAL_UINT64(ALLOCATION_TEST_MAX_FAILURE_INDEX, prime_allocations);
    TEST_ASSERT_LESS_OR_EQUAL_UINT64(ALLOCATION_TEST_MAX_FAILURE_INDEX, square_allocations);

    for (size_t failure_index = 1U; failure_index <= prime_allocations; failure_index++)
    {
        BigIntStatus status;

        result = true;
        numforge_test_allocator_begin(failure_index);
        status = bigint_is_probable_prime(&result, prime);
        TEST_ASSERT_TRUE(numforge_test_allocator_did_fail());
        numforge_test_allocator_end();
        TEST_ASSERT_EQUAL(BIGINT_OUT_OF_MEMORY, status);
        TEST_ASSERT_TRUE(result);
    }

    for (size_t failure_index = 1U; failure_index <= square_allocations; failure_index++)
    {
        BigIntStatus status;

        result = true;
        numforge_test_allocator_begin(failure_index);
        status = bigint_is_perfect_square(&result, square);
        TEST_ASSERT_TRUE(numforge_test_allocator_did_fail());
        numforge_test_allocator_end();
        TEST_ASSERT_EQUAL(BIGINT_OUT_OF_MEMORY, status);
        TEST_ASSERT_TRUE(result);
    }

    bigint_destroy(prime);
    bigint_destroy(square);
}

/*
------------------------------------------------------------------------------------------------------------------------------
    BigDecimal failure guarantees.
------------------------------------------------------------------------------------------------------------------------------
*/

#if SIZE_MAX == UINT32_MAX
void test_bigdecimal_format_size_overflow_preserves_output(void)
{
    static const char *const inputs[] = {
        "1e4294967294", "1e-4294967293",
        "-1e4294967293", "-1e-4294967292"
    };

    for (size_t index = 0U; index < sizeof(inputs) / sizeof(inputs[0]); index++)
    {
        BigDecimal *reference = make_bigdecimal(inputs[index][0] == '-' ? "-1" : "1");
        BigDecimal *value = make_bigdecimal(inputs[index]);
        char sentinel = 'x';
        char *text = NULL;
        BigDecimalStatus status;
        size_t output_allocation;
        bool injected;
        bool unchanged;

        /* The same coefficient takes the same conversion allocations. Fail
         * the final output allocation if a regression ever reaches it, so
         * the old malloc(0) bug reports failure instead of corrupting memory. */
        numforge_test_allocator_begin(0U);
        status = bigdecimal_to_string(reference, &text);
        output_allocation = numforge_test_allocator_call_count();
        numforge_test_allocator_end();
        free(text);
        bigdecimal_destroy(reference);
        if (status != BIGDECIMAL_OK || output_allocation == 0U)
        {
            bigdecimal_destroy(value);
            TEST_FAIL_MESSAGE("Failed to prepare the formatting allocation guard");
        }

        text = &sentinel;
        numforge_test_allocator_begin(output_allocation);
        status = bigdecimal_to_string(value, &text);
        injected = numforge_test_allocator_did_fail();
        numforge_test_allocator_end();
        unchanged = text == &sentinel;
        if (!unchanged) free(text);
        bigdecimal_destroy(value);

        TEST_ASSERT_EQUAL(BIGDECIMAL_VALUE_TOO_LARGE, status);
        TEST_ASSERT_FALSE(injected);
        TEST_ASSERT_TRUE(unchanged);
    }
}
#endif

void test_bigdecimal_from_bigint_every_allocation_failure(void)
{
    static const char input[] = "-123456789012345678901234567890000000000000000000000";
    BigInt *source = make_bigint(input);
    bool completed = false;
    for (size_t index = 1; index <= ALLOCATION_TEST_MAX_FAILURE_INDEX; index++)
    {
        BigDecimal *result = make_bigdecimal("7.77");
        numforge_test_allocator_begin(index);
        BigDecimalStatus status = bigdecimal_from_bigint(result, source);
        bool failed = numforge_test_allocator_did_fail();
        numforge_test_allocator_end();
        assert_bigint_text(input, source);
        if (failed)
        {
            TEST_ASSERT_EQUAL(BIGDECIMAL_OUT_OF_MEMORY, status);
            assert_bigdecimal_text("7.77", result);
        }
        else
        {
            TEST_ASSERT_EQUAL(BIGDECIMAL_OK, status);
            assert_bigdecimal_text(input, result);
            completed = true;
        }
        bigdecimal_destroy(result);
        if (completed) break;
    }
    bigint_destroy(source);
    TEST_ASSERT_TRUE(completed);
}

static void test_large_zero_normalization_failures_alias_and_scale(void)
{
    char input[4103];
    memcpy(input, "-12345", 6U); memset(input + 6U, '0', 4096U); input[4102] = '\0';
    BigInt *source = make_bigint(input);
    BigDecimal *expected = make_bigdecimal("-12345e4096");
    BigInt *original = make_bigint(input);
    for (unsigned alias = 0U; alias < 2U; ++alias)
    for (unsigned deadline = 0U; deadline < 2U; ++deadline)
    {
        bool completed = false;
        for (size_t index = 1U; index < 4096U; ++index)
        {
            BigDecimal *result = make_bigdecimal("42");
            if (alias) { TEST_ASSERT_EQUAL(BIGINT_OK, bigint_copy(result->coefficient, source)); result->scale = 0; }
            if (deadline)
            { (void)numforge_budget_begin(5000U, SIZE_MAX, SIZE_MAX); numforge_test_budget_expire_after(index); }
            else numforge_test_allocator_begin(index);
            BigDecimalStatus status = bigdecimal_from_bigint(result, alias ? result->coefficient : source);
            bool interrupted = deadline ? numforge_budget_failure() != NUMFORGE_BUDGET_OK : numforge_test_allocator_did_fail();
            numforge_budget_end(); numforge_test_allocator_end();
            TEST_ASSERT_EQUAL_INT(0, bigint_compare(source, original));
            if (status == BIGDECIMAL_OK)
            {
                int comparison = 1;
                TEST_ASSERT_FALSE(interrupted);
                TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_compare(&comparison, result, expected));
                TEST_ASSERT_EQUAL_INT(0, comparison); completed = true;
            }
            else
            {
                TEST_ASSERT_TRUE(interrupted); TEST_ASSERT_EQUAL(BIGDECIMAL_OUT_OF_MEMORY, status);
                if (alias) { TEST_ASSERT_EQUAL_INT(0, bigint_compare(source, result->coefficient)); TEST_ASSERT_EQUAL_INT64(0, result->scale); }
                else assert_bigdecimal_text("42", result);
            }
            bigdecimal_destroy(result);
            if (completed) break;
        }
        TEST_ASSERT_TRUE(completed);
    }
    /* A constrained optional scratch plan must fall back without poisoning. */
    BigInt copy = {0}; uint64_t removed = 0U;
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_copy(&copy, source));
    (void)numforge_budget_begin(5000U, 1U, 1U);
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_strip_decimal_zeros_inverse(&copy, &removed, 27U));
    TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_OK, numforge_budget_failure()); numforge_budget_end();
    TEST_ASSERT_EQUAL_UINT64(4096U, removed); assert_bigint_text("-12345", &copy); free(copy.limbs);
    char overflow[4130];
    memcpy(overflow, input, sizeof(input)); strcat(overflow, "e9223372036854775807");
    BigDecimal *result = make_bigdecimal("42");
    TEST_ASSERT_EQUAL(BIGDECIMAL_SCALE_OVERFLOW, bigdecimal_set_string(result, overflow));
    assert_bigdecimal_text("42", result);
    bigdecimal_destroy(result); bigdecimal_destroy(expected); bigint_destroy(source); bigint_destroy(original);
}

void test_bigdecimal_conversion_and_comparison_failure_paths(void)
{
    static const char large_value[] =
        "1234567890123456789012345678901234567890.12345678901234567890123456789";
    bool parse_completed = false;
    bool format_completed = false;
    bool compare_completed = false;

    for (size_t failure_index = 1U;
         failure_index <= ALLOCATION_TEST_MAX_FAILURE_INDEX;
         failure_index++)
    {
        BigDecimal *value = make_bigdecimal("7.77");
        BigDecimalStatus status;
        bool injected;

        numforge_test_allocator_begin(failure_index);
        status = bigdecimal_set_string(value, large_value);
        injected = numforge_test_allocator_did_fail();
        numforge_test_allocator_end();

        if (injected)
        {
            TEST_ASSERT_EQUAL(BIGDECIMAL_OUT_OF_MEMORY, status);
            assert_bigdecimal_text("7.77", value);
        }
        else
        {
            TEST_ASSERT_EQUAL(BIGDECIMAL_OK, status);
            parse_completed = true;
        }
        bigdecimal_destroy(value);
        if (parse_completed) break;
    }

    for (size_t failure_index = 1U;
         failure_index <= ALLOCATION_TEST_MAX_FAILURE_INDEX;
         failure_index++)
    {
        BigDecimal *value = make_bigdecimal(large_value);
        char sentinel = 'x';
        char *text = &sentinel;
        BigDecimalStatus status;
        bool injected;

        numforge_test_allocator_begin(failure_index);
        status = bigdecimal_to_string(value, &text);
        injected = numforge_test_allocator_did_fail();
        numforge_test_allocator_end();

        if (injected)
        {
            TEST_ASSERT_EQUAL(BIGDECIMAL_OUT_OF_MEMORY, status);
            TEST_ASSERT_EQUAL_PTR(&sentinel, text);
        }
        else
        {
            TEST_ASSERT_EQUAL(BIGDECIMAL_OK, status);
            TEST_ASSERT_EQUAL_STRING(large_value, text);
            format_completed = true;
            free(text);
        }
        bigdecimal_destroy(value);
        if (format_completed) break;
    }

    for (size_t failure_index = 1U;
         failure_index <= ALLOCATION_TEST_MAX_FAILURE_INDEX;
         failure_index++)
    {
        BigDecimal *a = make_bigdecimal(large_value);
        BigDecimal *b = make_bigdecimal("123456789012345678901234567890.5");
        int comparison = 73;
        BigDecimalStatus status;
        bool injected;

        numforge_test_allocator_begin(failure_index);
        status = bigdecimal_compare(&comparison, a, b);
        injected = numforge_test_allocator_did_fail();
        numforge_test_allocator_end();

        if (injected)
        {
            TEST_ASSERT_EQUAL(BIGDECIMAL_OUT_OF_MEMORY, status);
            TEST_ASSERT_EQUAL_INT(73, comparison);
        }
        else
        {
            TEST_ASSERT_EQUAL(BIGDECIMAL_OK, status);
            compare_completed = true;
        }
        bigdecimal_destroy(a);
        bigdecimal_destroy(b);
        if (compare_completed) break;
    }

    TEST_ASSERT_TRUE(parse_completed);
    TEST_ASSERT_TRUE(format_completed);
    TEST_ASSERT_TRUE(compare_completed);
}

void test_bigdecimal_arithmetic_failure_paths(void)
{
    static const char a[] = "123456789012345678901234567890.123456789";
    static const char b[] = "98765432109876543210.987654321";

    assert_bigdecimal_unary_failure_safety(bigdecimal_copy, a);
    assert_bigdecimal_unary_failure_safety(bigdecimal_abs, "-12345678901234567890.25");
    assert_bigdecimal_unary_failure_safety(bigdecimal_negate, a);
    assert_bigdecimal_unary_failure_safety(bigdecimal_rescale_to_25, a);
    assert_bigdecimal_unary_failure_safety(bigdecimal_floor, a);
    assert_bigdecimal_unary_failure_safety(bigdecimal_ceil, a);
    assert_bigdecimal_unary_failure_safety(bigdecimal_trunc, a);
    assert_bigdecimal_unary_failure_safety(bigdecimal_round_to_minus_5, a);
    assert_bigdecimal_unary_failure_safety(bigdecimal_exp_to_25, "1");
    assert_bigdecimal_unary_failure_safety(bigdecimal_ln_to_25, "2");
    assert_bigdecimal_binary_failure_safety(bigdecimal_add, a, b);
    assert_bigdecimal_binary_failure_safety(bigdecimal_min, a, b);
    assert_bigdecimal_binary_failure_safety(bigdecimal_max, a, b);
    assert_bigdecimal_binary_failure_safety(bigdecimal_sub, a, b);
    assert_bigdecimal_binary_failure_safety(bigdecimal_mul, a, b);
    assert_bigdecimal_binary_failure_safety(bigdecimal_sum_two, a, b);
    assert_bigdecimal_binary_failure_safety(bigdecimal_product_two, a, b);
    assert_bigdecimal_binary_failure_safety(bigdecimal_mean_two, a, b);
    assert_bigdecimal_binary_failure_safety(bigdecimal_median_two, a, b);
    assert_bigdecimal_binary_failure_safety(bigdecimal_geomean_two, a, b);
    assert_bigdecimal_binary_failure_safety(bigdecimal_harmean_two, a, b);
    assert_bigdecimal_binary_failure_safety(bigdecimal_variance_two, a, b);
    assert_bigdecimal_binary_failure_safety(bigdecimal_stdevp_two, a, b);
    assert_bigdecimal_binary_failure_safety(bigdecimal_stdev_two, a, b);
    assert_bigdecimal_binary_failure_safety(bigdecimal_divide_to_25, a, b);
}

void test_dynamic_constant_sampled_allocation_failures(void)
{
    size_t allocation_count;
    size_t failure_indices[7];
    BigDecimal *result = make_bigdecimal("7.77");
    BigDecimal *input = make_bigdecimal("0");

    numforge_test_allocator_begin(0U);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_pi_to_520(result, input));
    allocation_count = numforge_test_allocator_call_count();
    numforge_test_allocator_end();
    TEST_ASSERT_GREATER_THAN_UINT64(0U, allocation_count);
    bigdecimal_destroy(result);
    bigdecimal_destroy(input);

    failure_indices[0] = 1U;
    failure_indices[1] = 2U;
    failure_indices[2] = 8U;
    failure_indices[3] = allocation_count / 4U;
    failure_indices[4] = allocation_count / 2U;
    failure_indices[5] = allocation_count - allocation_count / 4U;
    failure_indices[6] = allocation_count;

    for (size_t index = 0U; index < sizeof(failure_indices) / sizeof(failure_indices[0]); index++)
    {
        size_t failure_index = failure_indices[index] == 0U ? 1U : failure_indices[index];
        BigDecimalStatus status;
        bool injected;

        result = make_bigdecimal("7.77");
        input = make_bigdecimal("0");
        numforge_test_allocator_begin(failure_index);
        status = bigdecimal_pi_to_520(result, input);
        injected = numforge_test_allocator_did_fail();
        numforge_test_allocator_end();

        TEST_ASSERT_TRUE(injected);
        TEST_ASSERT_EQUAL(BIGDECIMAL_OUT_OF_MEMORY, status);
        assert_bigdecimal_text("7.77", result);
        bigdecimal_destroy(result);
        bigdecimal_destroy(input);
    }
}

static void assert_sampled_unary_allocation_failures(
    BigDecimalUnaryOperation operation,
    const char *input_text
)
{
    size_t allocation_count;
    size_t failure_indices[7];
    BigDecimal *result = make_bigdecimal("7.77");
    BigDecimal *input = make_bigdecimal(input_text);

    numforge_test_allocator_begin(0U);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, operation(result, input));
    allocation_count = numforge_test_allocator_call_count();
    numforge_test_allocator_end();
    TEST_ASSERT_GREATER_THAN_UINT64(0U, allocation_count);
    bigdecimal_destroy(result);
    bigdecimal_destroy(input);

    failure_indices[0] = 1U;
    failure_indices[1] = 2U;
    failure_indices[2] = 8U;
    failure_indices[3] = allocation_count / 4U;
    failure_indices[4] = allocation_count / 2U;
    failure_indices[5] = allocation_count - allocation_count / 4U;
    failure_indices[6] = allocation_count;

    for (size_t index = 0U; index < sizeof(failure_indices) / sizeof(failure_indices[0]); index++)
    {
        size_t failure_index = failure_indices[index] == 0U ? 1U : failure_indices[index];
        BigDecimalStatus status;
        bool injected;

        result = make_bigdecimal("7.77");
        input = make_bigdecimal(input_text);
        numforge_test_allocator_begin(failure_index);
        status = operation(result, input);
        injected = numforge_test_allocator_did_fail();
        numforge_test_allocator_end();

        TEST_ASSERT_TRUE(injected);
        TEST_ASSERT_EQUAL(BIGDECIMAL_OUT_OF_MEMORY, status);
        assert_bigdecimal_text("7.77", result);
        bigdecimal_destroy(result);
        bigdecimal_destroy(input);
    }
}

void test_trigonometric_sampled_allocation_failures(void)
{
    assert_sampled_unary_allocation_failures(bigdecimal_sin_to_25, "0.5");
    assert_sampled_unary_allocation_failures(bigdecimal_atan_to_25, "1");
}

void test_signed_power_and_hyperbolic_sampled_allocation_failures(void)
{
    assert_sampled_unary_allocation_failures(bigdecimal_power_minus_three, "2");
    assert_sampled_unary_allocation_failures(bigdecimal_sinh_to_25, "0.5");
    assert_sampled_unary_allocation_failures(bigdecimal_cosh_to_25, "0.5");
    assert_sampled_unary_allocation_failures(bigdecimal_tanh_to_25, "0.5");
    assert_sampled_unary_allocation_failures(bigdecimal_asinh_to_25, "0.5");
    assert_sampled_unary_allocation_failures(bigdecimal_acosh_to_25, "2");
    assert_sampled_unary_allocation_failures(bigdecimal_atanh_to_25, "0.5");
}

void test_bigdecimal_aliasing_preserves_destination_on_allocation_failure(void)
{
    static const char a[] = "123456789012345678901234567890.123456789";
    static const char b[] = "98765432109876543210.987654321";

    assert_bigdecimal_binary_alias_failure_safety(bigdecimal_add, a, b);
    assert_bigdecimal_binary_alias_failure_safety(bigdecimal_mul, a, b);
    assert_bigdecimal_binary_alias_failure_safety(bigdecimal_sum_two, a, b);
    assert_bigdecimal_binary_alias_failure_safety(bigdecimal_product_two, a, b);
    assert_bigdecimal_binary_alias_failure_safety(bigdecimal_mean_two, a, b);
    assert_bigdecimal_binary_alias_failure_safety(bigdecimal_median_two, a, b);
    assert_bigdecimal_binary_alias_failure_safety(bigdecimal_geomean_two, a, b);
    assert_bigdecimal_binary_alias_failure_safety(bigdecimal_harmean_two, a, b);
    assert_bigdecimal_binary_alias_failure_safety(bigdecimal_variance_two, a, b);
    assert_bigdecimal_binary_alias_failure_safety(bigdecimal_stdevp_two, a, b);
    assert_bigdecimal_binary_alias_failure_safety(bigdecimal_stdev_two, a, b);
    assert_bigdecimal_binary_alias_failure_safety(bigdecimal_divide_to_25, a, b);
}

void test_logarithm_sampled_allocation_failures_and_aliasing(void)
{
    for (size_t failure_index = 1U; failure_index <= 128U; failure_index++)
    {
        BigDecimal *result = make_bigdecimal("7.77");
        BigDecimal *value = make_bigdecimal("8");
        BigDecimal *base = make_bigdecimal("2");

        numforge_test_allocator_begin(failure_index);
        BigDecimalStatus status = bigdecimal_log_to_25(result, value, base);
        bool injected = numforge_test_allocator_did_fail();
        numforge_test_allocator_end();

        TEST_ASSERT_TRUE(injected);
        TEST_ASSERT_EQUAL(BIGDECIMAL_OUT_OF_MEMORY, status);
        assert_bigdecimal_text("7.77", result);
        bigdecimal_destroy(result);
        bigdecimal_destroy(value);
        bigdecimal_destroy(base);
    }

    BigDecimal *value = make_bigdecimal("8");
    BigDecimal *base = make_bigdecimal("2");

    numforge_test_allocator_begin(0U);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_log_to_25(value, value, base));
    numforge_test_allocator_end();
    assert_bigdecimal_text("3", value);
    bigdecimal_destroy(value);
    bigdecimal_destroy(base);
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Calculator component contracts are checked directly as well as through the
    end-to-end adapter, so ownership and each layer's documented failure state
    remain explicit.
------------------------------------------------------------------------------------------------------------------------------
*/

void test_parser_preserves_output_on_every_allocation_failure(void)
{
    bool completed = false;

    for (size_t failure_index = 1U;
         failure_index <= ALLOCATION_TEST_MAX_FAILURE_INDEX;
         failure_index++)
    {
        char marker = 'x';
        CalculatorExpression *sentinel = (CalculatorExpression *)(void *)&marker;
        CalculatorExpression *expression = sentinel;
        CalculatorError error;
        CalculatorStatus status;
        bool injected;

        numforge_test_allocator_begin(failure_index);
        status = calculator_parse("min(pow(1.5;3);factorial(2);3;4;5;6) + 2", &expression, &error);
        injected = numforge_test_allocator_did_fail();
        numforge_test_allocator_end();

        if (injected)
        {
            TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY, status);
            TEST_ASSERT_EQUAL_PTR(sentinel, expression);
        }
        else
        {
            TEST_ASSERT_EQUAL(CALCULATOR_OK, status);
            TEST_ASSERT_TRUE(expression != sentinel);
            calculator_expression_destroy(expression);
            completed = true;
        }
        if (completed) break;
    }

    TEST_ASSERT_TRUE(completed);
}

void test_evaluator_preserves_destination_on_every_allocation_failure(void)
{
    bool completed = false;

    for (size_t failure_index = 1U;
         failure_index <= ALLOCATION_TEST_MAX_FAILURE_INDEX;
         failure_index++)
    {
        CalculatorExpression *expression = NULL;
        CalculatorContext context;
        CalculatorError error;
        BigDecimal *result;
        CalculatorStatus status;
        bool injected;

        TEST_ASSERT_EQUAL(
            CALCULATOR_OK,
            calculator_parse(
                "max(abs(pow(1.5;3));sign(-2);9) + factorial(2) + 7/28 + 1/3 + "
                "gcd(-48;18) + lcm(4;-6) + mod(-7;3) + npr(8;3) + ncr(8;3) + isqrt(999) + "
                "round(1.2345;2) + floor(-1.2) + ceil(1.2) + trunc(-1.2)",
                &expression,
                &error));
        result = make_bigdecimal("7.77");
        calculator_context_init(&context);

        numforge_test_allocator_begin(failure_index);
        status = calculator_evaluate(result, expression, &context, &error);
        injected = numforge_test_allocator_did_fail();
        numforge_test_allocator_end();

        if (injected)
        {
            TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY, status);
            assert_bigdecimal_text("7.77", result);
        }
        else
        {
            TEST_ASSERT_EQUAL(CALCULATOR_OK, status);
            completed = true;
        }

        bigdecimal_destroy(result);
        calculator_expression_destroy(expression);
        if (completed) break;
    }

    TEST_ASSERT_TRUE(completed);
}

void test_roots_preserve_aliases_on_every_allocation_failure(void)
{
    assert_bigint_unary_failure_safety(bigint_isqrt, "15241578750190521");
    static const struct { const char *input; uint32_t degree; } cases[] = {
        { "2", 2 }, { "-2", 3 }, { "0.0004", 2 }, { "81", 4 }, { "3", 1 }, { "0", 2 }
    };
    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++)
    {
        bool completed = false;
        for (size_t index = 1; index <= ALLOCATION_TEST_MAX_FAILURE_INDEX; index++)
        {
            BigDecimal *value = make_bigdecimal(cases[c].input);
            numforge_test_allocator_begin(index);
            BigDecimalStatus status = bigdecimal_root(value, value, cases[c].degree, 34, BIGDECIMAL_ROUND_HALF_EVEN);
            bool injected = numforge_test_allocator_did_fail();
            numforge_test_allocator_end();
            if (injected)
            {
                TEST_ASSERT_EQUAL(BIGDECIMAL_OUT_OF_MEMORY, status);
                assert_bigdecimal_text(cases[c].input, value);
            }
            else { TEST_ASSERT_EQUAL(BIGDECIMAL_OK, status); completed = true; }
            bigdecimal_destroy(value);
            if (completed) break;
        }
        TEST_ASSERT_TRUE(completed);
    }
}

void test_root_call_allocation_and_deadline_failures(void)
{
    CalculatorContext context;
    CalculatorError error;
    CalculatorExpression *expression = NULL;
    calculator_context_init(&context);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_parse("root(2;3)", &expression, &error));
    bool completed = false;
    for (size_t i = 1; i <= ALLOCATION_TEST_MAX_FAILURE_INDEX; i++)
    {
        BigDecimal *result = make_bigdecimal("7.77");
        numforge_test_allocator_begin(i);
        CalculatorStatus status = calculator_evaluate(result, expression, &context, &error);
        bool injected = numforge_test_allocator_did_fail();
        numforge_test_allocator_end();
        if (injected)
        {
            TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY, status);
            assert_bigdecimal_text("7.77", result);
        }
        else { TEST_ASSERT_EQUAL(CALCULATOR_OK, status); completed = true; }
        bigdecimal_destroy(result);
        if (completed) break;
    }
    calculator_expression_destroy(expression);
    TEST_ASSERT_TRUE(completed);
    completed = false;
    /* Cover entry/allocations densely and sample deeper BigInt loop checks. */
    for (size_t i = 1; i <= 65536U; i += i < 128U ? 1U : 97U)
    {
        char *text = NULL;
        numforge_test_budget_expire_after(i);
        CalculatorStatus status = calculator_compute("root(2;3)", &context, &text, &error);
        if (status == CALCULATOR_OK)
        {
            TEST_ASSERT_EQUAL_STRING("1.2599210499", text);
            free(text); completed = true; break;
        }
        TEST_ASSERT_NULL(text);
        TEST_ASSERT_EQUAL(CALCULATOR_TIME_LIMIT, status);
        TEST_ASSERT_EQUAL(CALCULATOR_TIME_LIMIT, error.status);
    }
    TEST_ASSERT_TRUE(completed);
}

void test_formatter_clears_output_on_every_allocation_failure(void)
{
    BigDecimal *value = make_bigdecimal("123456789012345678901234567890.123456789");
    CalculatorContext context;
    bool completed = false;

    calculator_context_init(&context);
    for (size_t failure_index = 1U;
         failure_index <= ALLOCATION_TEST_MAX_FAILURE_INDEX;
         failure_index++)
    {
        char marker = 'x';
        char *result = &marker;
        CalculatorStatus status;
        bool injected;

        numforge_test_allocator_begin(failure_index);
        status = calculator_format_result(value, &context, &result);
        injected = numforge_test_allocator_did_fail();
        numforge_test_allocator_end();

        if (injected)
        {
            TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY, status);
            TEST_ASSERT_NULL(result);
        }
        else
        {
            TEST_ASSERT_EQUAL(CALCULATOR_OK, status);
            TEST_ASSERT_NOT_NULL(result);
            free(result);
            completed = true;
        }
        if (completed) break;
    }

    bigdecimal_destroy(value);
    TEST_ASSERT_TRUE(completed);
}

/*
------------------------------------------------------------------------------------------------------------------------------
    End-to-end calculator allocation failures cover parser, evaluator, decimal
    arithmetic, and formatter allocations through the same adapter as the web.
------------------------------------------------------------------------------------------------------------------------------
*/

void test_calculator_pipeline_reports_every_injected_allocation_failure(void)
{
    bool completed = false;

    for (size_t failure_index = 1U;
         failure_index <= ALLOCATION_TEST_MAX_FAILURE_INDEX;
         failure_index++)
    {
        CalculatorError error;
        CalculatorStatus status;
        char *result = NULL;
        bool injected;

        numforge_test_allocator_begin(failure_index);
        status = numforge_web_evaluate("1.5^3 + 2 + 7/28 + 1/3", &result, &error);
        injected = numforge_test_allocator_did_fail();
        numforge_test_allocator_end();

        if (injected)
        {
            TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY, status);
            TEST_ASSERT_NULL(result);
        }
        else
        {
            TEST_ASSERT_EQUAL(CALCULATOR_OK, status);
            TEST_ASSERT_EQUAL_STRING("143/24", result);
            completed = true;
        }
        free(result);
        if (completed) break;
    }

    TEST_ASSERT_TRUE(completed);
}

void test_convert_every_allocation_failure(void)
{
    const char *inputs[] = {"qty(5;\"m\")*qty(5;\"cm\")", "qty(20;\"degC\")-qty(10;\"degF\")", "sqrt(qty(2;\"m2\"))", "convert(qty(1;\"km\");\"m\";\"cm\")", "convert(1/3;\"km\";\"m\")", "convert(sqrt(2);\"m\";\"cm\")", "convert(180;\"deg\";\"rad\")"};
    CalculatorContext context;
    calculator_context_init(&context);
    for (size_t i=0; i<sizeof(inputs)/sizeof(inputs[0]); i++)
    {
        bool completed=false;
        for (size_t n=1; n<=ALLOCATION_TEST_MAX_FAILURE_INDEX; n++)
        {
            char *text=NULL;
            CalculatorError error;
            numforge_test_allocator_begin(n);
            CalculatorStatus status=calculator_compute(inputs[i], &context, &text, &error);
            bool injected=numforge_test_allocator_did_fail();
            numforge_test_allocator_end();
            if (injected) { TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY, status); TEST_ASSERT_NULL(text); }
            else { TEST_ASSERT_EQUAL(CALCULATOR_OK, status); completed=true; }
            free(text);
            if (completed) break;
        }
        TEST_ASSERT_TRUE(completed);
    }
}

void test_application_budget_is_cumulative_and_scoped(void)
{
    void *first;
    void *second;
    bool owner = numforge_budget_begin(5000U, 24U, 16U);
    first = numforge_malloc(16U);
    free(first);
    second = numforge_malloc(16U);
    NumForgeBudgetFailure failure = numforge_budget_failure();
    bool nested = numforge_budget_begin(5000U, SIZE_MAX, SIZE_MAX);
    free(second);
    numforge_budget_end();
    TEST_ASSERT_TRUE(owner);
    TEST_ASSERT_FALSE(nested);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NULL(second);
    TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_MEMORY, failure);
    first = numforge_malloc(32U);
    TEST_ASSERT_NOT_NULL(first);
    free(first);
}

void test_grouped_factorial_preserves_alias_on_every_failure(void)
{
    static const char expected[] =
        "30414093201713378043612608166064768844377641568960512000000000000";
    for (unsigned deadline = 0; deadline < 2U; deadline++)
    {
        bool completed = false;
        for (size_t index = 1; index <= ALLOCATION_TEST_MAX_FAILURE_INDEX; index++)
        {
            BigInt *value = make_bigint("50");
            if (deadline)
            {
                (void)numforge_budget_begin(5000U, SIZE_MAX, SIZE_MAX);
                numforge_test_budget_expire_after(index);
            }
            else numforge_test_allocator_begin(index);
            BigIntStatus status = bigint_factorial(value, value);
            bool interrupted = deadline
                ? numforge_budget_failure() != NUMFORGE_BUDGET_OK
                : numforge_test_allocator_did_fail();
            numforge_budget_end();
            numforge_test_allocator_end();
            if (status == BIGINT_OK)
            {
                TEST_ASSERT_FALSE(interrupted);
                assert_bigint_text(expected, value);
                completed = true;
            }
            else
            {
                TEST_ASSERT_TRUE(interrupted);
                TEST_ASSERT_EQUAL(BIGINT_OUT_OF_MEMORY, status);
                assert_bigint_text("50", value);
            }
            bigint_destroy(value);
            if (completed) break;
        }
        TEST_ASSERT_TRUE(completed);
    }
}

void test_numeric_loops_cancel_without_changing_destinations(void)
{
    char digits[2001];
    BigInt *a;
    BigInt *b;
    BigInt *result;
    BigIntStatus status;
    char *text;
    memset(digits, '9', sizeof(digits) - 1U);
    digits[sizeof(digits) - 1U] = '\0';
    a = make_bigint(digits);
    b = make_bigint(digits);
    result = make_bigint("42");
    (void)numforge_budget_begin(5000U, SIZE_MAX, SIZE_MAX);
    numforge_test_budget_expire_after(3U);
    status = bigint_mul(result, a, b);
    numforge_budget_end();
    TEST_ASSERT_EQUAL(BIGINT_OUT_OF_MEMORY, status);
    assert_bigint_text("42", result);
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(b, "3"));
    (void)numforge_budget_begin(5000U, SIZE_MAX, SIZE_MAX);
    numforge_test_budget_expire_after(20U);
    status = bigint_div(result, a, b);
    numforge_budget_end();
    TEST_ASSERT_EQUAL(BIGINT_OUT_OF_MEMORY, status);
    assert_bigint_text("42", result);
    (void)numforge_budget_begin(5000U, SIZE_MAX, SIZE_MAX);
    numforge_test_budget_expire_after(3U);
    text = bigint_to_string(a);
    numforge_budget_end();
    free(text);
    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
    TEST_ASSERT_NULL(text);
}

void test_pipeline_deadline_covers_all_checkpoints(void)
{
    CalculatorContext context;
    bool completed = false;
    calculator_context_init(&context);
    for (size_t index = 1U; index <= ALLOCATION_TEST_MAX_FAILURE_INDEX; index++)
    {
        CalculatorError error;
        char *text = NULL;
        numforge_test_budget_expire_after(index);
        CalculatorStatus status = calculator_compute("7/28+gcd(-48;18)+lcm(4;-6)+mod(-7;3)+isqrt(999)", &context, &text, &error);
        if (status == CALCULATOR_OK)
        {
            TEST_ASSERT_EQUAL_STRING("48.25", text);
            free(text);
            completed = true;
            break;
        }
        TEST_ASSERT_NULL(text);
        TEST_ASSERT_EQUAL(CALCULATOR_TIME_LIMIT, status);
        TEST_ASSERT_EQUAL(CALCULATOR_TIME_LIMIT, error.status);
    }
    TEST_ASSERT_TRUE(completed);
}

void test_cache_preserves_value_on_every_allocation_failure(void)
{
    for (size_t scenario = 0U; scenario < 2U; scenario++)
    {
        bool completed = false;
        for (size_t index = 1U; index <= ALLOCATION_TEST_MAX_FAILURE_INDEX; index++)
        {
            NumForgeWebCache cache = {0};
            CalculatorError error;
            char *text = NULL;
            bool reused;
            CalculatorStatus status = numforge_web_evaluate_cached(
                &cache, 1U, "42", 10, CALCULATOR_ANGLE_RADIANS, &text, &error, &reused);
            TEST_ASSERT_EQUAL(CALCULATOR_OK, status);
            free(text);
            text = NULL;
            numforge_test_allocator_begin(index);
            status = numforge_web_evaluate_cached(&cache, 2U, scenario == 0U ? "42" : "1/3",
                50, CALCULATOR_ANGLE_RADIANS, &text, &error, &reused);
            bool failed = numforge_test_allocator_did_fail();
            numforge_test_allocator_end();
            if (failed)
            {
                TEST_ASSERT_NOT_EQUAL(CALCULATOR_OK, status);
                TEST_ASSERT_EQUAL(status, error.status);
                TEST_ASSERT_NULL(text);
                TEST_ASSERT_EQUAL_STRING("42", cache.expression);
                TEST_ASSERT_FALSE(reused);
            }
            else
            {
                TEST_ASSERT_EQUAL(CALCULATOR_OK, status);
                completed = true;
            }
            free(text);
            numforge_web_cache_clear(&cache);
            if (completed)
            {
                break;
            }
        }
        TEST_ASSERT_TRUE(completed);
    }
}

void test_bigrational_preserves_aliases_on_every_allocation_failure(void)
{
    typedef BigRationalStatus (*Operation)(
        BigRational *, const BigRational *, const BigRational *);
    const Operation operations[] = {
        bigrational_add, bigrational_sub, bigrational_mul, bigrational_div
    };

    for (size_t operation = 0U; operation < sizeof(operations) / sizeof(operations[0]); operation++)
    {
        bool completed = false;
        for (size_t index = 1U; index <= ALLOCATION_TEST_MAX_FAILURE_INDEX; index++)
        {
            BigRational *a = make_bigrational("12345678901234567890", "99999999999999999999");
            BigRational *b = make_bigrational("9876543210987654321", "88888888888888888889");
            BigRationalStatus status;
            bool failed;

            numforge_test_allocator_begin(index);
            status = operations[operation](a, a, b);
            failed = numforge_test_allocator_did_fail();
            numforge_test_allocator_end();

            if (failed)
            {
                TEST_ASSERT_EQUAL(BIGRATIONAL_OUT_OF_MEMORY, status);
                assert_bigrational_text("137174210/1111111111", a);
            }
            else
            {
                TEST_ASSERT_EQUAL(BIGRATIONAL_OK, status);
                completed = true;
            }
            bigrational_destroy(a);
            bigrational_destroy(b);
            if (completed) break;
        }
        TEST_ASSERT_TRUE(completed);
    }
}

void test_bigrational_conversion_preserves_outputs_on_allocation_failure(void)
{
    for (size_t scenario = 0U; scenario < 3U; scenario++)
    {
        bool completed = false;
        for (size_t index = 1U; index <= ALLOCATION_TEST_MAX_FAILURE_INDEX; index++)
        {
            BigRational *rational = make_bigrational("7", "8");
            BigDecimal *decimal = make_bigdecimal(scenario == 0U ? "1.25" : "9");
            char *text = (char *)"unchanged";
            char *original_text = text;
            BigRationalStatus status;
            bool failed;

            numforge_test_allocator_begin(index);
            if (scenario == 0U)
            {
                status = bigrational_from_bigdecimal(rational, decimal);
            }
            else if (scenario == 1U)
            {
                status = bigrational_to_bigdecimal(decimal, rational, 20, BIGDECIMAL_ROUND_HALF_EVEN);
            }
            else
            {
                status = bigrational_to_string(rational, &text);
            }
            failed = numforge_test_allocator_did_fail();
            numforge_test_allocator_end();

            if (failed)
            {
                TEST_ASSERT_EQUAL(BIGRATIONAL_OUT_OF_MEMORY, status);
                assert_bigrational_text("7/8", rational);
                if (scenario == 1U) assert_bigdecimal_text("9", decimal);
                if (scenario == 2U) TEST_ASSERT_EQUAL_PTR(original_text, text);
            }
            else
            {
                TEST_ASSERT_EQUAL(BIGRATIONAL_OK, status);
                completed = true;
            }
            if (scenario == 2U && status == BIGRATIONAL_OK) free(text);
            bigdecimal_destroy(decimal);
            bigrational_destroy(rational);
            if (completed) break;
        }
        TEST_ASSERT_TRUE(completed);
    }
}


static void test_base_conversion_and_prefix_failure_safety(void)
{
    char hexadecimal[1001], decimal[4097];
    memset(hexadecimal,'f',1000);hexadecimal[1000]='\0';
    memset(decimal,'7',4096);decimal[4096]='\0';
    BigInt *value=make_bigint("777");
    BigDecimal *large=make_bigdecimal(decimal);
    char marker_text[]="unchanged";
    char *text=marker_text;
    size_t parse_calls, format_calls;
    numforge_test_allocator_begin(0);
    TEST_ASSERT_EQUAL(BIGINT_OK,bigint_set_string_base(value,hexadecimal,16));
    parse_calls=numforge_test_allocator_call_count();numforge_test_allocator_end();
    for(size_t failure=1;failure<=parse_calls;failure++)
    {
        TEST_ASSERT_EQUAL(BIGINT_OK,bigint_set_string(value,"777"));
        numforge_test_allocator_begin(failure);
        BigIntStatus status=bigint_set_string_base(value,hexadecimal,16);
        TEST_ASSERT_TRUE(numforge_test_allocator_did_fail());numforge_test_allocator_end();
        TEST_ASSERT_EQUAL(BIGINT_OUT_OF_MEMORY,status);assert_bigint_text("777",value);
    }
    for(size_t failure=1;failure<=2;failure++)
    {
        text=marker_text;
        numforge_test_allocator_begin(failure);
        BigIntStatus status=bigint_to_string_base(value,2,false,&text);
        TEST_ASSERT_TRUE(numforge_test_allocator_did_fail());numforge_test_allocator_end();
        TEST_ASSERT_EQUAL(BIGINT_OUT_OF_MEMORY,status);TEST_ASSERT_EQUAL_PTR(marker_text,text);
    }
    text=NULL;numforge_test_allocator_begin(0);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_format(large,10,BIGDECIMAL_ROUND_HALF_EVEN,&text));
    format_calls=numforge_test_allocator_call_count();numforge_test_allocator_end();free(text);
    for(size_t failure=1;failure<=format_calls;failure++)
    {
        text=marker_text;
        numforge_test_allocator_begin(failure);
        BigDecimalStatus status=bigdecimal_format(large,10,BIGDECIMAL_ROUND_HALF_EVEN,&text);
        TEST_ASSERT_TRUE(numforge_test_allocator_did_fail());numforge_test_allocator_end();
        TEST_ASSERT_EQUAL(BIGDECIMAL_OUT_OF_MEMORY,status);TEST_ASSERT_EQUAL_PTR(marker_text,text);
    }
    bigint_destroy(value);bigdecimal_destroy(large);
}

static void test_series_sum_owners_preserve_alias_on_every_failure(void)
{
    static const struct {
        BigDecimalUnaryOperation operation;
        const char *input, *expected;
    } cases[] = {
        { bigdecimal_ln_to_25, "2", "0.6931471805599453094172321" },
        { bigdecimal_exp_to_25, "1", "2.718281828459045235360287" }
    };
    for (size_t c = 0U; c < sizeof(cases) / sizeof(cases[0]); c++)
    for (unsigned deadline = 0U; deadline < 2U; deadline++)
    {
        bool completed = false;
        for (size_t index = 1U; index <= ALLOCATION_TEST_MAX_FAILURE_INDEX; index++)
        {
            BigDecimal *value = make_bigdecimal(cases[c].input);
            BigDecimalStatus status;
            bool interrupted;
            if (deadline)
            {
                (void)numforge_budget_begin(5000U, SIZE_MAX, SIZE_MAX);
                numforge_test_budget_expire_after(index);
            }
            else numforge_test_allocator_begin(index);
            status = cases[c].operation(value, value);
            interrupted = deadline ? numforge_budget_failure() != NUMFORGE_BUDGET_OK
                                   : numforge_test_allocator_did_fail();
            numforge_budget_end();
            numforge_test_allocator_end();
            if (status == BIGDECIMAL_OK)
            {
                TEST_ASSERT_FALSE(interrupted);
                assert_bigdecimal_text(cases[c].expected, value);
                completed = true;
            }
            else
            {
                TEST_ASSERT_TRUE(interrupted);
                TEST_ASSERT_EQUAL(BIGDECIMAL_OUT_OF_MEMORY, status);
                assert_bigdecimal_text(cases[c].input, value);
            }
            bigdecimal_destroy(value);
            if (completed) break;
        }
        TEST_ASSERT_TRUE(completed);
    }
}

static void test_equal_scale_comparison_without_allocations(void)
{
    static const struct { const char *a, *b; int expected; } cases[] = {
        { "1.25", "2.75", -1 }, { "-1.25", "-2.75", 1 },
        { "-2.75", "-1.25", -1 }, { "1.25", "1.25", 0 },
        { "18446744073709551617", "18446744073709551616", 1 }
    };
    for (size_t c = 0U; c < sizeof(cases) / sizeof(cases[0]); c++)
    {
        BigDecimal *a = make_bigdecimal(cases[c].a);
        BigDecimal *b = make_bigdecimal(cases[c].b);
        int comparison = 42;
        numforge_test_allocator_begin(1U);
        BigDecimalStatus status = bigdecimal_compare(&comparison, a, b);
        bool allocated = numforge_test_allocator_call_count() != 0U;
        numforge_test_allocator_end();
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, status);
        TEST_ASSERT_FALSE(allocated);
        TEST_ASSERT_EQUAL_INT(cases[c].expected, comparison);
        comparison = 42;
        (void)numforge_budget_begin(5000U, SIZE_MAX, SIZE_MAX);
        numforge_test_budget_expire_after(1U);
        status = bigdecimal_compare(&comparison, a, b);
        numforge_budget_end();
        TEST_ASSERT_EQUAL(BIGDECIMAL_OUT_OF_MEMORY, status);
        TEST_ASSERT_EQUAL_INT(42, comparison);
        assert_bigdecimal_text(cases[c].a, a);
        assert_bigdecimal_text(cases[c].b, b);
        bigdecimal_destroy(a);
        bigdecimal_destroy(b);
    }
}

static BigDecimalStatus workspace_operation(unsigned operation, BigDecimal *result,
    const BigDecimal *a, const BigDecimal *b, int64_t digits, BigDecimalWorkspace *workspace)
{
    switch (operation)
    {
        case 0: return bigdecimal_round_significant_workspace(result, a, digits, BIGDECIMAL_ROUND_HALF_EVEN, workspace);
        case 1: return bigdecimal_div_significant_workspace(result, a, b, digits, BIGDECIMAL_ROUND_HALF_EVEN, workspace);
        case 2: return bigdecimal_add_significant_workspace(result, a, b, digits, BIGDECIMAL_ROUND_HALF_EVEN, workspace);
        default: return bigdecimal_mul_significant_workspace(result, a, b, digits, BIGDECIMAL_ROUND_HALF_EVEN, workspace);
    }
}

static void test_decimal_workspace_every_failure_alias_and_retry(void)
{
    static const struct { unsigned operation; const char *a, *b; int64_t digits; const char *expected; } cases[] = {
        { 0U, "1234.56789", "-0.12345", 6, "1234.57" },
        { 0U, "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789.5", "37.125", 9, "123456789000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" },
        { 1U, "1234.56789", "-0.12345", 6, "-10000.5" },
        { 1U, "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789.5", "37.125", 9, "3325435390000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" },
        { 2U, "1234.56789", "-0.12345", 6, "1234.44" },
        { 2U, "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789.5", "37.125", 9, "123456789000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" },
        { 3U, "1234.56789", "-0.12345", 6, "-152.407" },
        { 3U, "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789.5", "37.125", 9, "4583333290000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" },
    };
    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++)
    for (unsigned alias = 0; alias < 3U; alias++)
    for (unsigned warm = 0; warm < 3U; warm++)
    for (unsigned deadline = 0; deadline < 2U; deadline++)
    {
        bool completed = false;
        for (size_t index = 1; index <= ALLOCATION_TEST_MAX_FAILURE_INDEX; index++)
        {
            BigDecimalWorkspace workspace = {0};
            BigDecimal *a = make_bigdecimal(cases[c].a), *b = make_bigdecimal(cases[c].b);
            BigDecimal *original = make_bigdecimal("7.77"), *warm_result = make_bigdecimal("0");
            BigDecimal *result = alias == 1 ? a : alias == 2 ? b : original;
            if (warm == 1)
            {
                BigDecimal *seed = make_bigdecimal("1234567890123456789012345678901234567890.5");
                TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_round_significant_workspace(
                    warm_result, seed, 5, BIGDECIMAL_ROUND_HALF_EVEN, &workspace));
                bigdecimal_destroy(seed);
            }
            if (warm == 2) TEST_ASSERT_EQUAL(BIGDECIMAL_OK, workspace_operation(
                cases[c].operation, warm_result, a, b, cases[c].digits, &workspace));
            if (deadline)
            {
                (void)numforge_budget_begin(5000U, SIZE_MAX, SIZE_MAX);
                numforge_test_budget_expire_after(index);
            }
            else numforge_test_allocator_begin(index);
            BigDecimalStatus status = workspace_operation(cases[c].operation, result, a, b, cases[c].digits, &workspace);
            bool interrupted = deadline ? numforge_budget_failure() != NUMFORGE_BUDGET_OK : numforge_test_allocator_did_fail();
            numforge_budget_end(); numforge_test_allocator_end();
            if (status == BIGDECIMAL_OK)
            {
                TEST_ASSERT_FALSE(interrupted);
                assert_bigdecimal_text(cases[c].expected, result);
                if (result != a) assert_bigdecimal_text(cases[c].a, a);
                if (result != b) assert_bigdecimal_text(cases[c].b, b);
                completed = true;
            }
            else
            {
                TEST_ASSERT_TRUE(interrupted);
                TEST_ASSERT_EQUAL(BIGDECIMAL_OUT_OF_MEMORY, status);
                assert_bigdecimal_text(cases[c].a, a); assert_bigdecimal_text(cases[c].b, b);
                assert_bigdecimal_text("7.77", original);
                /* Partial scratch must remain reusable, including invalidated caches. */
                TEST_ASSERT_EQUAL(BIGDECIMAL_OK, workspace_operation(
                    cases[c].operation, result, a, b, cases[c].digits, &workspace));
                assert_bigdecimal_text(cases[c].expected, result);
            }
            bigdecimal_workspace_clear(&workspace);
            bigdecimal_workspace_clear(&workspace);
            bigdecimal_destroy(a); bigdecimal_destroy(b); bigdecimal_destroy(original); bigdecimal_destroy(warm_result);
            if (completed) break;
        }
        TEST_ASSERT_TRUE(completed);
    }
}

static void test_decimal_workspace_scale_boundaries_and_invalid_arguments(void)
{
    BigDecimalWorkspace workspace = {0};
    BigDecimal *a = make_bigdecimal("2"), *b = make_bigdecimal("5"), *result = make_bigdecimal("7.77");
    a->scale = INT64_MAX; b->scale = 1;
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_mul_significant_workspace(result, a, b, 10,
        BIGDECIMAL_ROUND_HALF_EVEN, &workspace));
    TEST_ASSERT_EQUAL_INT64(INT64_MAX, result->scale);
    assert_bigint_text("1", result->coefficient);
    a->scale = INT64_MIN; b->scale = INT64_MIN;
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_div_significant_workspace(result, a, b, 10,
        BIGDECIMAL_ROUND_HALF_EVEN, &workspace));
    assert_bigdecimal_text("0.4", result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_SCALE_OVERFLOW, bigdecimal_mul_significant_workspace(result, a, b, 10,
        BIGDECIMAL_ROUND_HALF_EVEN, &workspace));
    assert_bigdecimal_text("0.4", result);
    for (unsigned op = 0; op < 4U; op++)
    {
        TEST_ASSERT_EQUAL(BIGDECIMAL_INVALID_ARGUMENT, workspace_operation(op, result, a, b, 0, &workspace));
        assert_bigdecimal_text("0.4", result);
    }
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_round_significant_workspace(result, a, 10,
        BIGDECIMAL_ROUND_HALF_EVEN, NULL));
    a->scale = 0; b->scale = 1000000000;
    (void)numforge_budget_begin(UINT64_MAX, 4096U, 2048U);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OUT_OF_MEMORY, bigdecimal_add_significant_workspace(result, a, b, 10,
        BIGDECIMAL_ROUND_HALF_EVEN, &workspace));
    TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_MEMORY, numforge_budget_failure());
    numforge_budget_end();
    assert_bigdecimal_text("0.4", result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(b, "0"));
    TEST_ASSERT_EQUAL(BIGDECIMAL_DIVISION_BY_ZERO, bigdecimal_div_significant_workspace(result, a, b, 10,
        BIGDECIMAL_ROUND_HALF_EVEN, &workspace));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_mul_significant_workspace(result, a, b, 10,
        BIGDECIMAL_ROUND_HALF_EVEN, &workspace));
    assert_bigdecimal_text("0", result);
    bigdecimal_workspace_clear(&workspace);
    bigdecimal_destroy(a); bigdecimal_destroy(b); bigdecimal_destroy(result);
}

static void test_decimal_borrowed_coefficients_every_failure(void)
{
    static const struct {
        const char *a, *b, *expected;
        unsigned operation; // 0 add, 1 subtract, 2 fixed, 3 significant division
        int64_t scale;
        BigDecimalRoundingMode rounding;
    } cases[] = {
        { "1.25", "2.75", "4", 0, 0, BIGDECIMAL_ROUND_HALF_EVEN },
        { "1.25", "2", "3.25", 0, 0, BIGDECIMAL_ROUND_HALF_EVEN },
        { "2", "1.25", "0.75", 1, 0, BIGDECIMAL_ROUND_HALF_EVEN },
        { "1.25", "2.75", "-1.5", 1, 0, BIGDECIMAL_ROUND_HALF_EVEN },
        { "1", "8", "0.12", 2, 2, BIGDECIMAL_ROUND_HALF_EVEN },
        { "1", "-8", "-0.13", 2, 2, BIGDECIMAL_ROUND_HALF_UP },
        { "150", "20", "10", 2, -1, BIGDECIMAL_ROUND_HALF_EVEN },
        { "7", "-2", "-3", 2, 0, BIGDECIMAL_ROUND_TOWARD_ZERO },
        { "7", "-2", "-4", 2, 0, BIGDECIMAL_ROUND_AWAY_FROM_ZERO },
        { "-7", "2", "-4", 2, 0, BIGDECIMAL_ROUND_FLOOR },
        { "-7", "2", "-3", 2, 0, BIGDECIMAL_ROUND_CEILING },
        { "-7", "2", "-4", 2, 0, BIGDECIMAL_ROUND_HALF_UP },
        { "-5", "2", "-2", 2, 0, BIGDECIMAL_ROUND_HALF_EVEN },
        { "5", "-2", "-2", 2, 0, BIGDECIMAL_ROUND_HALF_EVEN },
        { "100000000000000000001", "1", "100000000000000000000", 3, 19, BIGDECIMAL_ROUND_HALF_EVEN },
        { "1", "100000000000000000001", "0.00000000000000000001", 3, 3, BIGDECIMAL_ROUND_HALF_EVEN },
        { "100000000000000000001", "100000000000000000003", "1", 3, 19, BIGDECIMAL_ROUND_HALF_EVEN }
    };
    for (size_t c = 0U; c < sizeof(cases) / sizeof(cases[0]); c++)
    for (unsigned alias = 0U; alias < 3U; alias++)
    for (unsigned deadline = 0U; deadline < 2U; deadline++)
    {
        bool completed = false;
        for (size_t index = 1U; index <= ALLOCATION_TEST_MAX_FAILURE_INDEX; index++)
        {
            BigDecimal *a = make_bigdecimal(cases[c].a);
            BigDecimal *b = make_bigdecimal(cases[c].b);
            BigDecimal *original = make_bigdecimal("7.77");
            BigDecimal *result = alias == 1U ? a : alias == 2U ? b : original;
            BigDecimalStatus status;
            bool interrupted;
            if (deadline)
            {
                (void)numforge_budget_begin(5000U, SIZE_MAX, SIZE_MAX);
                numforge_test_budget_expire_after(index);
            }
            else numforge_test_allocator_begin(index);
            status = cases[c].operation == 0U ? bigdecimal_add(result, a, b)
                   : cases[c].operation == 1U ? bigdecimal_sub(result, a, b)
                   : cases[c].operation == 2U ? bigdecimal_div(result, a, b, cases[c].scale, cases[c].rounding)
                   : bigdecimal_div_significant(result, a, b, cases[c].scale, cases[c].rounding);
            interrupted = deadline ? numforge_budget_failure() != NUMFORGE_BUDGET_OK
                                   : numforge_test_allocator_did_fail();
            numforge_budget_end();
            numforge_test_allocator_end();
            if (status == BIGDECIMAL_OK)
            {
                TEST_ASSERT_FALSE(interrupted);
                assert_bigdecimal_text(cases[c].expected, result);
                if (result != a) assert_bigdecimal_text(cases[c].a, a);
                if (result != b) assert_bigdecimal_text(cases[c].b, b);
                completed = true;
            }
            else
            {
                TEST_ASSERT_TRUE(interrupted);
                TEST_ASSERT_EQUAL(BIGDECIMAL_OUT_OF_MEMORY, status);
                assert_bigdecimal_text(cases[c].a, a);
                assert_bigdecimal_text(cases[c].b, b);
                assert_bigdecimal_text("7.77", original);
            }
            bigdecimal_destroy(a);
            bigdecimal_destroy(b);
            bigdecimal_destroy(original);
            if (completed) break;
        }
        TEST_ASSERT_TRUE(completed);
    }
}

static void test_decimal_digit_count_preserves_output_on_every_failure(void)
{
    char large[2002];
    memset(large, '0', sizeof(large));
    large[0] = '1'; large[2000] = '1'; large[2001] = '\0';
    const char *inputs[] = { "0", "18446744073709551616", "-100000000000000000001", large };
    const size_t expected[] = { 1U, 20U, 21U, 2001U };
    for (size_t c = 0U; c < sizeof(inputs) / sizeof(inputs[0]); c++)
    for (unsigned deadline = 0U; deadline < 2U; deadline++)
    {
        bool completed = false;
        BigInt *value = make_bigint(inputs[c]);
        for (size_t index = 1U; index <= ALLOCATION_TEST_MAX_FAILURE_INDEX; index++)
        {
            size_t count = 42U;
            BigIntStatus status;
            bool interrupted;
            if (deadline)
            {
                (void)numforge_budget_begin(5000U, SIZE_MAX, SIZE_MAX);
                numforge_test_budget_expire_after(index);
            }
            else numforge_test_allocator_begin(index);
            status = bigint_decimal_digits(value, &count);
            interrupted = deadline ? numforge_budget_failure() != NUMFORGE_BUDGET_OK
                                   : numforge_test_allocator_did_fail();
            numforge_budget_end(); numforge_test_allocator_end();
            assert_bigint_text(inputs[c], value);
            if (status == BIGINT_OK)
            {
                TEST_ASSERT_FALSE(interrupted);
                TEST_ASSERT_EQUAL_UINT64(expected[c], count);
                completed = true;
            }
            else
            {
                TEST_ASSERT_TRUE(interrupted);
                TEST_ASSERT_EQUAL(BIGINT_OUT_OF_MEMORY, status);
                TEST_ASSERT_EQUAL_UINT64(42U, count);
            }
            if (completed) break;
        }
        bigint_destroy(value);
        TEST_ASSERT_TRUE(completed);
    }
}

static void check_forward_pair_failures(bool hyperbolic)
{
    const char *trig_inputs[] = { "0", "1" };
    const char *hyperbolic_inputs[] = { "0", "0.1", "-1" };
    const char **inputs = hyperbolic ? hyperbolic_inputs : trig_inputs;
    const size_t count = hyperbolic ? 3U : 2U;
    const int64_t digits = hyperbolic ? 4 : 12;
    BigDecimalStatus (*pair)(BigDecimal *, BigDecimal *, const BigDecimal *, int64_t,
                            BigDecimalRoundingMode) = hyperbolic
        ? bigdecimal_sinh_cosh_pair : bigdecimal_sin_cos_pair;
    BigDecimalStatus (*sine)(BigDecimal *, const BigDecimal *, int64_t,
                            BigDecimalRoundingMode) = hyperbolic ? bigdecimal_sinh : bigdecimal_sin;
    BigDecimalStatus (*cosine)(BigDecimal *, const BigDecimal *, int64_t,
                              BigDecimalRoundingMode) = hyperbolic ? bigdecimal_cosh : bigdecimal_cos;
    for (size_t n = 0U; n < count; n++)
    for (unsigned alias = 0U; alias < 3U; alias++)
    for (unsigned deadline = 0U; deadline < 2U; deadline++)
    {
        BigDecimal *expected_s = bigdecimal_create(), *expected_c = bigdecimal_create();
        BigDecimal *input = make_bigdecimal(inputs[n]);
        bool completed = false;
        TEST_ASSERT_NOT_NULL(expected_s); TEST_ASSERT_NOT_NULL(expected_c);
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, sine(expected_s, input, digits, BIGDECIMAL_ROUND_HALF_EVEN));
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, cosine(expected_c, input, digits, BIGDECIMAL_ROUND_HALF_EVEN));
        bigdecimal_destroy(input);
        /* Exp plus the small-argument sinh series has more checkpoints than
         * the generic 4096-position bound. Require eventual success as well. */
        for (size_t index = 1U; index <= 16384U; index++)
        {
            BigDecimal *value = make_bigdecimal(inputs[n]);
            BigDecimal *original_s = make_bigdecimal("7.77"), *original_c = make_bigdecimal("-8.88");
            BigDecimal *s = alias == 1U ? value : original_s;
            BigDecimal *c = alias == 2U ? value : original_c;
            BigDecimalStatus status;
            bool interrupted;
            if (deadline)
            {
                (void)numforge_budget_begin(5000U, SIZE_MAX, SIZE_MAX);
                numforge_test_budget_expire_after(index);
            }
            else numforge_test_allocator_begin(index);
            status = pair(s, c, value, digits, BIGDECIMAL_ROUND_HALF_EVEN);
            interrupted = deadline ? numforge_budget_failure() != NUMFORGE_BUDGET_OK
                                   : numforge_test_allocator_did_fail();
            numforge_budget_end(); numforge_test_allocator_end();
            if (status == BIGDECIMAL_OK)
            {
                int comparison = 42;
                TEST_ASSERT_FALSE(interrupted);
                TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_compare(&comparison, s, expected_s));
                TEST_ASSERT_EQUAL_INT(0, comparison);
                TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_compare(&comparison, c, expected_c));
                TEST_ASSERT_EQUAL_INT(0, comparison);
                if (alias == 0U) assert_bigdecimal_text(inputs[n], value);
                completed = true;
            }
            else
            {
                TEST_ASSERT_TRUE(interrupted);
                TEST_ASSERT_EQUAL(BIGDECIMAL_OUT_OF_MEMORY, status);
                assert_bigdecimal_text(inputs[n], value);
                assert_bigdecimal_text("7.77", original_s);
                assert_bigdecimal_text("-8.88", original_c);
            }
            bigdecimal_destroy(value); bigdecimal_destroy(original_s); bigdecimal_destroy(original_c);
            if (completed) break;
        }
        bigdecimal_destroy(expected_s); bigdecimal_destroy(expected_c);
        TEST_ASSERT_TRUE(completed);
    }
    BigDecimal *a = make_bigdecimal("1"), *b = make_bigdecimal("2");
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, pair(NULL, b, a, digits, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, pair(a, NULL, b, digits, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, pair(a, b, NULL, digits, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_INVALID_ARGUMENT, pair(a, a, b, digits, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_INVALID_ARGUMENT, pair(a, b, a, 0, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_VALUE_TOO_LARGE, pair(a, b, a, INT64_MAX, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_INVALID_ARGUMENT, pair(a, b, a, digits, (BigDecimalRoundingMode)99));
    assert_bigdecimal_text("1", a); assert_bigdecimal_text("2", b);
    bigdecimal_destroy(a); bigdecimal_destroy(b);
}

static void test_sin_cos_pair_atomic_on_every_failure(void)
{
    check_forward_pair_failures(false);
}

static void test_sinh_cosh_pair_atomic_on_every_failure(void)
{
    check_forward_pair_failures(true);
}

void test_large_factorial_cancellation_preserves_destination(void)
{
    const char *inputs[] = { "100001", "1000000", "18446744073709551615" };
    for (size_t i = 0U; i < sizeof(inputs) / sizeof(inputs[0]); i++)
    for (unsigned alias = 0U; alias < 2U; alias++)
    {
        BigInt *n = make_bigint(inputs[i]), *out = make_bigint("77");
        numforge_test_budget_expire_after(1000U);
        TEST_ASSERT_TRUE(numforge_budget_begin(UINT64_MAX, SIZE_MAX, SIZE_MAX));
        BigIntStatus status = bigint_factorial(alias ? n : out, n);
        TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_TIME, numforge_budget_failure());
        numforge_budget_end();
        TEST_ASSERT_EQUAL(BIGINT_OUT_OF_MEMORY, status);
        assert_bigint_text(inputs[i], n);
        assert_bigint_text("77", out);
        bigint_destroy(n); bigint_destroy(out);
    }
    /* Expire inside, rather than before, a long private limb multiplication. */
    BigInt scratch = {0};
    scratch.size = scratch.capacity = 1024U;
    scratch.limbs = calloc(scratch.capacity, sizeof(uint64_t));
    TEST_ASSERT_NOT_NULL(scratch.limbs);
    for (size_t i = 0U; i < scratch.size; i++) scratch.limbs[i] = UINT64_MAX;
    numforge_test_budget_expire_after(2U);
    TEST_ASSERT_TRUE(numforge_budget_begin(UINT64_MAX, SIZE_MAX, SIZE_MAX));
    TEST_ASSERT_EQUAL(BIGINT_OUT_OF_MEMORY, bigint_multiply_by_uint64(&scratch, 3U));
    TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_TIME, numforge_budget_failure());
    numforge_budget_end();
    free(scratch.limbs);
    const char *expressions[] = { "100001!", "factorial(100001)", "1000000!", "18446744073709551615!" };
    for (size_t i = 0U; i < sizeof(expressions) / sizeof(expressions[0]); i++)
    {
        CalculatorContext context;
        CalculatorError error;
        char *text = NULL;
        calculator_context_init(&context);
        numforge_test_budget_expire_after(1000U);
        TEST_ASSERT_EQUAL(CALCULATOR_TIME_LIMIT, calculator_compute(expressions[i], &context, &text, &error));
        TEST_ASSERT_EQUAL(CALCULATOR_TIME_LIMIT, error.status);
        TEST_ASSERT_NULL(text);
    }
}

static void test_factorial_tree_failure_cleanup_and_budget_fallback(void)
{
    BigInt *input = make_bigint("8192"), *expected = bigint_create();
    TEST_ASSERT_NOT_NULL(expected);
    /* Scalar path supplies an additional independent algorithm;
     * complete external factorial references are checked by the harness. */
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_factorial_grouped(expected, 8192U));
    for (unsigned alias = 0U; alias < 2U; ++alias)
    {
        bool completed = false;
        for (size_t fail = 1U; fail < ALLOCATION_TEST_MAX_FAILURE_INDEX; ++fail)
        {
            BigInt *out = make_bigint(alias ? "8192" : "77");
            numforge_test_allocator_begin(fail);
            BigIntStatus status = bigint_factorial_tree(out, 8192U);
            bool failed = numforge_test_allocator_did_fail();
            numforge_test_allocator_end();
            if (status == BIGINT_OK)
            {
                TEST_ASSERT_FALSE(failed);
                TEST_ASSERT_EQUAL_INT(0, bigint_compare(out, expected));
                completed = true;
            }
            else
            {
                TEST_ASSERT_TRUE(failed);
                TEST_ASSERT_EQUAL(BIGINT_OUT_OF_MEMORY, status);
                assert_bigint_text(alias ? "8192" : "77", out);
            }
            bigint_destroy(out);
            if (completed) break;
        }
        TEST_ASSERT_TRUE(completed);
    }
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_uint64(input, 2000U));
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_factorial_grouped(expected, 2000U));
    bool completed = false;
    for (size_t fail = 1U; fail < 50000U; ++fail)
    {
        BigInt *out = make_bigint("2000");
        TEST_ASSERT_TRUE(numforge_budget_begin(UINT64_MAX, SIZE_MAX, SIZE_MAX));
        numforge_test_budget_expire_after(fail);
        BigIntStatus status = bigint_factorial_tree(out, 2000U);
        NumForgeBudgetFailure reason = numforge_budget_failure();
        numforge_budget_end();
        if (status == BIGINT_OK)
        {
            TEST_ASSERT_EQUAL_INT(0, bigint_compare(out, expected));
            completed = true;
        }
        else
        {
            TEST_ASSERT_EQUAL(BIGINT_OUT_OF_MEMORY, status);
            TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_TIME, reason);
            assert_bigint_text("2000", out);
        }
        bigint_destroy(out);
        if (completed) break;
    }
    TEST_ASSERT_TRUE(completed);
    /* Optional tree planning must neither consume nor poison a scope. */
    TEST_ASSERT_TRUE(numforge_budget_begin(UINT64_MAX, 65536U, 65536U));
    TEST_ASSERT_FALSE(bigint_factorial_tree_fits(8192U));
    TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_OK, numforge_budget_failure());
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_uint64(input, 8192U));
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_factorial(expected, input));
    TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_OK, numforge_budget_failure());
    numforge_budget_end();
    bigint_destroy(input); bigint_destroy(expected);
}

static void test_production_agm_cancellation_preserves_destination(void)
{
    for (unsigned alias = 0; alias < 2; alias++) {
        BigDecimal *input = make_bigdecimal("2"), *out = make_bigdecimal("77");
        numforge_test_budget_expire_after(1000U);
        TEST_ASSERT_TRUE(numforge_budget_begin(UINT64_MAX, SIZE_MAX, SIZE_MAX));
        TEST_ASSERT_EQUAL(BIGDECIMAL_OUT_OF_MEMORY,
            bigdecimal_ln(alias ? input : out, input, 2000, BIGDECIMAL_ROUND_HALF_EVEN));
        TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_TIME, numforge_budget_failure());
        numforge_budget_end();
        assert_bigdecimal_text("2", input); assert_bigdecimal_text("77", out);
        bigdecimal_destroy(input); bigdecimal_destroy(out);
    }
}

/* Independent radix identity: (B^128-1)^2=B^256-2*B^128+1,
 * B=2^64. This exercises the production entry boundary without a second
 * multiplication implementation supplying the expected value. */
static void test_karatsuba_dispatch_alias_failures_and_budget_fallback(void)
{
    BigInt input = {0}, expected = {0}, a = {0}, b = {0}, result = {0}, original = {0};
    input.size = input.capacity = 128U;
    input.limbs = calloc(128U, sizeof(uint64_t));
    expected.size = expected.capacity = 256U;
    expected.limbs = calloc(256U, sizeof(uint64_t));
    TEST_ASSERT_NOT_NULL(input.limbs);
    TEST_ASSERT_NOT_NULL(expected.limbs);
    for (size_t i = 0U; i < 128U; ++i) input.limbs[i] = UINT64_MAX;
    expected.limbs[0] = 1U;
    expected.limbs[128] = UINT64_MAX - 1U;
    for (size_t i = 129U; i < 256U; ++i) expected.limbs[i] = UINT64_MAX;
    for (unsigned mode = 0U; mode < 4U; ++mode)
    {
        BigInt *out = mode == 1U || mode == 3U ? &a : mode == 2U ? &b : &result;
        for (unsigned kind = 0U; kind < 2U; ++kind)
        {
            bool succeeded = false;
            for (size_t fail = 1U; fail < 20000U; ++fail)
            {
                TEST_ASSERT_EQUAL(BIGINT_OK, bigint_copy(&a, &input));
                TEST_ASSERT_EQUAL(BIGINT_OK, bigint_copy(&b, &input));
                TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_uint64(&result, 77U));
                TEST_ASSERT_EQUAL(BIGINT_OK, bigint_copy(&original, out));
                TEST_ASSERT_TRUE(numforge_budget_begin(UINT64_MAX, SIZE_MAX, SIZE_MAX));
                numforge_test_allocator_begin(kind == 0U ? fail : 0U);
                if (kind == 1U) numforge_test_budget_expire_after(fail);
                BigIntStatus status = bigint_mul(out, &a, mode == 3U ? &a : &b);
                bool allocation_failed = numforge_test_allocator_did_fail();
                NumForgeBudgetFailure reason = numforge_budget_failure();
                numforge_test_allocator_end(); numforge_budget_end();
                if (status == BIGINT_OK)
                {
                    TEST_ASSERT_EQUAL_INT(0, bigint_compare(out, &expected));
                    if (kind == 0U) TEST_ASSERT_EQUAL_UINT(3U, fail); /* Two owned buffers. */
                    succeeded = true; break;
                }
                TEST_ASSERT_EQUAL(BIGINT_OUT_OF_MEMORY, status);
                TEST_ASSERT_EQUAL_INT(0, bigint_compare(out, &original));
                TEST_ASSERT_TRUE(kind == 0U ? allocation_failed : reason == NUMFORGE_BUDGET_TIME);
            }
            TEST_ASSERT_TRUE(succeeded);
        }
        for (unsigned limit = 0U; limit < 3U; ++limit)
        {
            const size_t bytes = 256U * sizeof(uint64_t);
            TEST_ASSERT_EQUAL(BIGINT_OK, bigint_copy(&a, &input));
            TEST_ASSERT_EQUAL(BIGINT_OK, bigint_copy(&b, &input));
            TEST_ASSERT_TRUE(numforge_budget_begin(UINT64_MAX,
                limit == 0U ? bytes : limit == 2U ? bytes + 1U : SIZE_MAX,
                limit == 1U ? bytes : SIZE_MAX));
            void *spent = limit == 2U ? numforge_malloc(1U) : NULL;
            if (limit == 2U) TEST_ASSERT_NOT_NULL(spent);
            numforge_test_allocator_begin(0U);
            TEST_ASSERT_EQUAL(BIGINT_OK, bigint_mul(out, &a, mode == 3U ? &a : &b));
            TEST_ASSERT_EQUAL_UINT(1U, numforge_test_allocator_call_count());
            TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_OK, numforge_budget_failure());
            numforge_test_allocator_end(); numforge_budget_end(); free(spent);
            TEST_ASSERT_EQUAL_INT(0, bigint_compare(out, &expected));
        }
    }
    free(input.limbs); free(expected.limbs); free(a.limbs); free(b.limbs);
    free(result.limbs); free(original.limbs);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_convert_every_allocation_failure);
    RUN_TEST(test_base_conversion_and_prefix_failure_safety);
    RUN_TEST(test_application_budget_is_cumulative_and_scoped);
    RUN_TEST(test_numeric_loops_cancel_without_changing_destinations);
    RUN_TEST(test_grouped_factorial_preserves_alias_on_every_failure);
    RUN_TEST(test_factorial_tree_failure_cleanup_and_budget_fallback);
    RUN_TEST(test_pipeline_deadline_covers_all_checkpoints);
    RUN_TEST(test_roots_preserve_aliases_on_every_allocation_failure);
    RUN_TEST(test_root_call_allocation_and_deadline_failures);

    RUN_TEST(test_allocator_injects_malloc_calloc_and_realloc_failures);
    RUN_TEST(test_numeric_creation_cleans_up_every_failed_allocation);
    RUN_TEST(test_bigint_conversion_failure_paths);
    RUN_TEST(test_bigint_arithmetic_failure_paths);
    RUN_TEST(test_karatsuba_dispatch_alias_failures_and_budget_fallback);
    RUN_TEST(test_bigint_divmod_and_number_theory_failure_paths);
    RUN_TEST(test_limb_division_every_failure_and_alias);
    RUN_TEST(test_gcd_workspace_every_failure_and_alias);
    RUN_TEST(test_gcd_large_pair_fits_production_allocation_budget);
    RUN_TEST(test_bigint_bitwise_and_shift_failure_paths);
    RUN_TEST(test_bigint_aliasing_preserves_destination_on_allocation_failure);
    RUN_TEST(test_bigint_boolean_number_theory_handles_every_allocation_failure);
#if SIZE_MAX == UINT32_MAX
    RUN_TEST(test_bigdecimal_format_size_overflow_preserves_output);
#endif
    RUN_TEST(test_bigdecimal_conversion_and_comparison_failure_paths);
    RUN_TEST(test_bigdecimal_from_bigint_every_allocation_failure);
    RUN_TEST(test_large_zero_normalization_failures_alias_and_scale);
    RUN_TEST(test_bigdecimal_arithmetic_failure_paths);
    RUN_TEST(test_decimal_workspace_every_failure_alias_and_retry);
    RUN_TEST(test_decimal_workspace_scale_boundaries_and_invalid_arguments);
    RUN_TEST(test_decimal_borrowed_coefficients_every_failure);
    RUN_TEST(test_equal_scale_comparison_without_allocations);
    RUN_TEST(test_decimal_digit_count_preserves_output_on_every_failure);
    RUN_TEST(test_sin_cos_pair_atomic_on_every_failure);
    RUN_TEST(test_sinh_cosh_pair_atomic_on_every_failure);
    RUN_TEST(test_series_sum_owners_preserve_alias_on_every_failure);
    RUN_TEST(test_bigdecimal_aliasing_preserves_destination_on_allocation_failure);
    RUN_TEST(test_dynamic_constant_sampled_allocation_failures);
    RUN_TEST(test_trigonometric_sampled_allocation_failures);
    RUN_TEST(test_signed_power_and_hyperbolic_sampled_allocation_failures);
    RUN_TEST(test_logarithm_sampled_allocation_failures_and_aliasing);
    RUN_TEST(test_parser_preserves_output_on_every_allocation_failure);
    RUN_TEST(test_evaluator_preserves_destination_on_every_allocation_failure);
    RUN_TEST(test_formatter_clears_output_on_every_allocation_failure);
    RUN_TEST(test_calculator_pipeline_reports_every_injected_allocation_failure);
    RUN_TEST(test_cache_preserves_value_on_every_allocation_failure);
    RUN_TEST(test_bigrational_preserves_aliases_on_every_allocation_failure);
    RUN_TEST(test_bigrational_conversion_preserves_outputs_on_allocation_failure);

    RUN_TEST(test_large_factorial_cancellation_preserves_destination);
    RUN_TEST(test_production_agm_cancellation_preserves_destination);
    return UNITY_END();
}
