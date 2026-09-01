#include <stdlib.h>

#include <unity.h>

#include <numforge/bigdecimal.h>
#include <numforge/bigint.h>

#include "numforge_alloc.h"
#include "evaluator.h"
#include "formatter.h"
#include "parser.h"
#include "web_api.h"

#ifndef NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING
#error "allocation_failure_tests requires NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING"
#endif

#define ALLOCATION_TEST_MAX_FAILURE_INDEX 1024U

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
    numforge_test_allocator_end();
}

void tearDown(void)
{
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

static BigDecimalStatus bigdecimal_divide_to_25(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b
)
{
    return bigdecimal_div(result, a, b, 25, BIGDECIMAL_ROUND_HALF_EVEN);
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

    numforge_test_allocator_begin(0U);
    TEST_ASSERT_TRUE(bigint_is_probable_prime(prime));
    prime_allocations = numforge_test_allocator_call_count();
    numforge_test_allocator_end();

    numforge_test_allocator_begin(0U);
    TEST_ASSERT_TRUE(bigint_is_perfect_square(square));
    square_allocations = numforge_test_allocator_call_count();
    numforge_test_allocator_end();

    TEST_ASSERT_GREATER_THAN_UINT64(0U, prime_allocations);
    TEST_ASSERT_GREATER_THAN_UINT64(0U, square_allocations);
    TEST_ASSERT_LESS_OR_EQUAL_UINT64(ALLOCATION_TEST_MAX_FAILURE_INDEX, prime_allocations);
    TEST_ASSERT_LESS_OR_EQUAL_UINT64(ALLOCATION_TEST_MAX_FAILURE_INDEX, square_allocations);

    for (size_t failure_index = 1U; failure_index <= prime_allocations; failure_index++)
    {
        bool result;

        numforge_test_allocator_begin(failure_index);
        result = bigint_is_probable_prime(prime);
        TEST_ASSERT_TRUE(numforge_test_allocator_did_fail());
        numforge_test_allocator_end();
        TEST_ASSERT_FALSE(result);
    }

    for (size_t failure_index = 1U; failure_index <= square_allocations; failure_index++)
    {
        bool result;

        numforge_test_allocator_begin(failure_index);
        result = bigint_is_perfect_square(square);
        TEST_ASSERT_TRUE(numforge_test_allocator_did_fail());
        numforge_test_allocator_end();
        TEST_ASSERT_FALSE(result);
    }

    bigint_destroy(prime);
    bigint_destroy(square);
}

/*
------------------------------------------------------------------------------------------------------------------------------
    BigDecimal failure guarantees.
------------------------------------------------------------------------------------------------------------------------------
*/

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
    assert_bigdecimal_binary_failure_safety(bigdecimal_add, a, b);
    assert_bigdecimal_binary_failure_safety(bigdecimal_sub, a, b);
    assert_bigdecimal_binary_failure_safety(bigdecimal_mul, a, b);
    assert_bigdecimal_binary_failure_safety(bigdecimal_divide_to_25, a, b);
}

void test_bigdecimal_aliasing_preserves_destination_on_allocation_failure(void)
{
    static const char a[] = "123456789012345678901234567890.123456789";
    static const char b[] = "98765432109876543210.987654321";

    assert_bigdecimal_binary_alias_failure_safety(bigdecimal_add, a, b);
    assert_bigdecimal_binary_alias_failure_safety(bigdecimal_mul, a, b);
    assert_bigdecimal_binary_alias_failure_safety(bigdecimal_divide_to_25, a, b);
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
        status = calculator_parse("1.5^3 + 2", &expression, &error);
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

        TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_parse("1.5^3 + 2", &expression, &error));
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
        status = numforge_web_evaluate("1.5^3 + 2", &result, &error);
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
            TEST_ASSERT_EQUAL_STRING("5.375", result);
            completed = true;
        }
        free(result);
        if (completed) break;
    }

    TEST_ASSERT_TRUE(completed);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_allocator_injects_malloc_calloc_and_realloc_failures);
    RUN_TEST(test_numeric_creation_cleans_up_every_failed_allocation);
    RUN_TEST(test_bigint_conversion_failure_paths);
    RUN_TEST(test_bigint_arithmetic_failure_paths);
    RUN_TEST(test_bigint_divmod_and_number_theory_failure_paths);
    RUN_TEST(test_bigint_bitwise_and_shift_failure_paths);
    RUN_TEST(test_bigint_aliasing_preserves_destination_on_allocation_failure);
    RUN_TEST(test_bigint_boolean_number_theory_handles_every_allocation_failure);
    RUN_TEST(test_bigdecimal_conversion_and_comparison_failure_paths);
    RUN_TEST(test_bigdecimal_arithmetic_failure_paths);
    RUN_TEST(test_bigdecimal_aliasing_preserves_destination_on_allocation_failure);
    RUN_TEST(test_parser_preserves_output_on_every_allocation_failure);
    RUN_TEST(test_evaluator_preserves_destination_on_every_allocation_failure);
    RUN_TEST(test_formatter_clears_output_on_every_allocation_failure);
    RUN_TEST(test_calculator_pipeline_reports_every_injected_allocation_failure);

    return UNITY_END();
}
