#include <unity.h>

#include "calculator_internal.h"
#include "tokenizer.h"

void setUp(void)
{
}

void tearDown(void)
{
}

/* ============================================================
   Shared calculator utilities
   ============================================================ */

void test_context_defaults_and_status_strings(void)
{
    CalculatorContext context;

    calculator_context_init(&context);

    TEST_ASSERT_EQUAL_INT64(34, context.division_scale);
    TEST_ASSERT_EQUAL(BIGDECIMAL_ROUND_HALF_EVEN, context.rounding);
    TEST_ASSERT_EQUAL_STRING("syntax error", calculator_status_to_string(CALCULATOR_SYNTAX_ERROR));
    TEST_ASSERT_EQUAL_STRING("unknown status", calculator_status_to_string((CalculatorStatus)999));
}

void test_error_helpers(void)
{
    CalculatorError error;

    calculator_error_set(&error, CALCULATOR_INVALID_TOKEN, 7);
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_TOKEN, error.status);
    TEST_ASSERT_EQUAL_UINT(7, error.offset);

    calculator_error_clear(&error);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, error.status);
    TEST_ASSERT_EQUAL_UINT(0, error.offset);
}

/* ============================================================
   Tokenizer
   ============================================================ */

void test_tokenizer_stub_has_stable_contract(void)
{
    CalculatorTokenizer tokenizer;
    CalculatorToken token;
    CalculatorError error;

    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_tokenizer_init(&tokenizer, "1 + 2"));
    TEST_ASSERT_EQUAL_UINT(5, tokenizer.length);
    TEST_ASSERT_EQUAL_UINT(0, tokenizer.offset);
    TEST_ASSERT_EQUAL(CALCULATOR_NOT_IMPLEMENTED,
                      calculator_tokenizer_next(&tokenizer, &token, &error));
    TEST_ASSERT_EQUAL(CALCULATOR_NOT_IMPLEMENTED, error.status);
    TEST_ASSERT_EQUAL_UINT(0, error.offset);
    TEST_ASSERT_EQUAL(CALCULATOR_NULL_ARGUMENT, calculator_tokenizer_init(NULL, "1"));
}

/* ============================================================
   Main
   ============================================================ */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_context_defaults_and_status_strings);
    RUN_TEST(test_error_helpers);
    RUN_TEST(test_tokenizer_stub_has_stable_contract);

    return UNITY_END();
}
