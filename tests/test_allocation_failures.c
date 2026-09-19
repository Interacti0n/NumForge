#include <stdlib.h>
#include <string.h>

#include <unity.h>

#include <numforge/bigdecimal.h>
#include <numforge/bigint.h>

#include "numforge_alloc.h"
#include "evaluator.h"
#include "formatter.h"
#include "parser.h"
#include <numforge/bigdecimal.h>
#include "calculator_internal.h"
#include "web_api.h"

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

void test_bigdecimal_aliasing_preserves_destination_on_allocation_failure(void)
{
    static const char a[] = "123456789012345678901234567890.123456789";
    static const char b[] = "98765432109876543210.987654321";

    assert_bigdecimal_binary_alias_failure_safety(bigdecimal_add, a, b);
    assert_bigdecimal_binary_alias_failure_safety(bigdecimal_mul, a, b);
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
            TEST_ASSERT_EQUAL_STRING("5.9583333333", result);
            completed = true;
        }
        free(result);
        if (completed) break;
    }

    TEST_ASSERT_TRUE(completed);
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
    for (size_t index = 1U; index <= 1024U; index++)
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

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_application_budget_is_cumulative_and_scoped);
    RUN_TEST(test_numeric_loops_cancel_without_changing_destinations);
    RUN_TEST(test_pipeline_deadline_covers_all_checkpoints);
    RUN_TEST(test_roots_preserve_aliases_on_every_allocation_failure);
    RUN_TEST(test_root_call_allocation_and_deadline_failures);

    RUN_TEST(test_allocator_injects_malloc_calloc_and_realloc_failures);
    RUN_TEST(test_numeric_creation_cleans_up_every_failed_allocation);
    RUN_TEST(test_bigint_conversion_failure_paths);
    RUN_TEST(test_bigint_arithmetic_failure_paths);
    RUN_TEST(test_bigint_divmod_and_number_theory_failure_paths);
    RUN_TEST(test_bigint_bitwise_and_shift_failure_paths);
    RUN_TEST(test_bigint_aliasing_preserves_destination_on_allocation_failure);
    RUN_TEST(test_bigint_boolean_number_theory_handles_every_allocation_failure);
#if SIZE_MAX == UINT32_MAX
    RUN_TEST(test_bigdecimal_format_size_overflow_preserves_output);
#endif
    RUN_TEST(test_bigdecimal_conversion_and_comparison_failure_paths);
    RUN_TEST(test_bigdecimal_arithmetic_failure_paths);
    RUN_TEST(test_bigdecimal_aliasing_preserves_destination_on_allocation_failure);
    RUN_TEST(test_dynamic_constant_sampled_allocation_failures);
    RUN_TEST(test_trigonometric_sampled_allocation_failures);
    RUN_TEST(test_logarithm_sampled_allocation_failures_and_aliasing);
    RUN_TEST(test_parser_preserves_output_on_every_allocation_failure);
    RUN_TEST(test_evaluator_preserves_destination_on_every_allocation_failure);
    RUN_TEST(test_formatter_clears_output_on_every_allocation_failure);
    RUN_TEST(test_calculator_pipeline_reports_every_injected_allocation_failure);
    RUN_TEST(test_cache_preserves_value_on_every_allocation_failure);

    return UNITY_END();
}
