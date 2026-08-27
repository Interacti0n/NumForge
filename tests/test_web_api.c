#include <stdlib.h>
#include <string.h>

#include <unity.h>

#include "web_api.h"

void setUp(void)
{
}

void tearDown(void)
{
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Local web adapter tests.
------------------------------------------------------------------------------------------------------------------------------
*/
void test_web_api_evaluates_with_exact_c_bigdecimal(void)
{
    CalculatorError error;
    char *result = NULL;

    TEST_ASSERT_EQUAL(CALCULATOR_OK, numforge_web_evaluate("0.1 + 0.2", &result, &error));
    TEST_ASSERT_EQUAL(CALCULATOR_OK, error.status);
    TEST_ASSERT_EQUAL_STRING("0.3", result);
    free(result);

    result = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, numforge_web_evaluate("(12.5 - 2.5) / 4", &result, &error));
    TEST_ASSERT_EQUAL_STRING("2.5", result);
    free(result);

    result = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, numforge_web_evaluate("1,25E-1 + .25", &result, &error));
    TEST_ASSERT_EQUAL_STRING("0.375", result);
    free(result);

    result = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, numforge_web_evaluate("\xCF\x80", &result, &error));
    TEST_ASSERT_EQUAL_STRING("3.1415926536", result);
    free(result);

    result = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, numforge_web_evaluate("1e3 - 3e + 1E3 - 1000", &result, &error));
    TEST_ASSERT_EQUAL_STRING("0", result);
    free(result);

    result = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, numforge_web_evaluate("1.5\xC2\xB2 + 2\xC2\xB3 + 5!", &result, &error));
    TEST_ASSERT_EQUAL_STRING("130.25", result);
    free(result);
}

void test_web_api_honors_output_precision(void)
{
    CalculatorError error;
    char *result = NULL;

    TEST_ASSERT_EQUAL(CALCULATOR_OK,
                      numforge_web_evaluate_with_output_scale("1 / 3", 3, &result, &error));
    TEST_ASSERT_EQUAL_STRING("0.333", result);
    free(result);

    result = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,
                      numforge_web_evaluate_with_output_scale("1.234567890123E-45", 10, &result, &error));
    TEST_ASSERT_EQUAL_STRING("1.2345678901E-45", result);
    free(result);

    result = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,
                      numforge_web_evaluate_with_output_scale("1E100000", 10, &result, &error));
    TEST_ASSERT_EQUAL_STRING("1E+100000", result);
    free(result);

    result = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,
                      numforge_web_evaluate_with_output_scale("\xCF\x86", CALCULATOR_UNLIMITED_OUTPUT_SCALE,
                                                               &result, &error));
    TEST_ASSERT_EQUAL_STRING_LEN("1.61803398874989484820", result, 22);
    free(result);
}

void test_web_api_preserves_calculator_errors(void)
{
    CalculatorError error;
    char *result = NULL;

    TEST_ASSERT_EQUAL(CALCULATOR_DIVISION_BY_ZERO, numforge_web_evaluate("1 / 0", &result, &error));
    TEST_ASSERT_NULL(result);
    TEST_ASSERT_EQUAL(CALCULATOR_DIVISION_BY_ZERO, error.status);
    TEST_ASSERT_EQUAL_UINT(2, error.offset);

    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_TOKEN, numforge_web_evaluate("2 + hello", &result, &error));
    TEST_ASSERT_NULL(result);
    TEST_ASSERT_EQUAL_UINT(4, error.offset);

    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT, numforge_web_evaluate("1.5!", &result, &error));
    TEST_ASSERT_NULL(result);
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT, error.status);
    TEST_ASSERT_EQUAL_UINT(3, error.offset);
}

void test_web_api_rejects_empty_and_oversized_input(void)
{
    CalculatorError error;
    char *result = NULL;
    char input[NUMFORGE_WEB_MAX_EXPRESSION_LENGTH + 2U];

    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_TOO_LARGE, numforge_web_evaluate("", &result, &error));
    TEST_ASSERT_NULL(result);

    memset(input, '1', sizeof(input) - 1U);
    input[sizeof(input) - 1U] = '\0';
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_TOO_LARGE, numforge_web_evaluate(input, &result, &error));
    TEST_ASSERT_NULL(result);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_web_api_evaluates_with_exact_c_bigdecimal);
    RUN_TEST(test_web_api_honors_output_precision);
    RUN_TEST(test_web_api_preserves_calculator_errors);
    RUN_TEST(test_web_api_rejects_empty_and_oversized_input);

    return UNITY_END();
}
