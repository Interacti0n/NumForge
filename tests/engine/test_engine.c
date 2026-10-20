#include <unity.h>
#include <numforge/engine.h>
#include <numforge/runtime.h>
#include "src/runtime/numforge_alloc.h"
#include "src/engine/api/calculator_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) { numforge_budget_end(); numforge_test_allocator_end(); }
void tearDown(void) { numforge_test_allocator_end(); numforge_budget_end(); }

static void check_text(const NumForgeResult *result, const char *expected)
{
    char *text = NULL;
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_result_to_string(result, &text));
    TEST_ASSERT_EQUAL_STRING(expected, text);
    free(text);
}

static void test_exact_typed_results(void)
{
    const char *inputs[] = {"2+2", "0.1+0.2", "1/3", "sqrt(-1)", "2^128"};
    const char *outputs[] = {"4", "3/10", "1/3", "i", "340282366920938463463374607431768211456"};
    const NumForgeResultKind kinds[] = {NUMFORGE_RESULT_INTEGER, NUMFORGE_RESULT_RATIONAL,
        NUMFORGE_RESULT_RATIONAL, NUMFORGE_RESULT_COMPLEX_RATIONAL, NUMFORGE_RESULT_INTEGER};
    for (size_t i = 0; i < sizeof(inputs)/sizeof(inputs[0]); ++i) {
        NumForgeResult *result = NULL;
        NumForgeError error = {NUMFORGE_SYNTAX_ERROR, 99U};
        TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_evaluate(&result, inputs[i], NULL, &error));
        TEST_ASSERT_EQUAL(NUMFORGE_OK, error.status);
        TEST_ASSERT_EQUAL_UINT(0U, error.offset);
        TEST_ASSERT_TRUE(numforge_result_is_exact(result));
        TEST_ASSERT_EQUAL(kinds[i], numforge_result_kind(result));
        TEST_ASSERT_EQUAL(kinds[i] == NUMFORGE_RESULT_INTEGER, numforge_result_integer(result) != NULL);
        TEST_ASSERT_EQUAL(kinds[i] == NUMFORGE_RESULT_RATIONAL, numforge_result_rational(result) != NULL);
        TEST_ASSERT_EQUAL(kinds[i] == NUMFORGE_RESULT_COMPLEX_RATIONAL, numforge_result_complex_rational(result) != NULL);
        TEST_ASSERT_NULL(numforge_result_decimal(result));
        TEST_ASSERT_NULL(numforge_result_complex_decimal(result));
        check_text(result, outputs[i]);
        numforge_result_destroy(result);
    }
}

static void test_prepared_ownership_and_precision(void)
{
    NumForgeContext *context = numforge_context_create();
    NumForgeExpression *expression = NULL;
    NumForgeResult *first = NULL, *second = NULL;
    char input[] = "sqrt(2)";
    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_expression_prepare(&expression, input, NULL, NULL));
    memset(input, 'x', sizeof(input)-1U);
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_context_set_precision(context, 34));
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_expression_evaluate(&first, expression, context, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_context_set_precision(context, 50));
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_expression_evaluate(&second, expression, context, NULL));
    numforge_expression_destroy(expression);
    numforge_context_destroy(context);
    TEST_ASSERT_FALSE(numforge_result_is_exact(first));
    TEST_ASSERT_NOT_NULL(numforge_result_decimal(first));
    char *text = NULL;
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_to_string(numforge_result_decimal(first), &text));
    TEST_ASSERT_EQUAL_STRING("1.414213562373095048801688724209698", text);
    free(text); text = NULL;
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_to_string(numforge_result_decimal(second), &text));
    TEST_ASSERT_EQUAL_STRING("1.4142135623730950488016887242096980785696718753769", text);
    free(text);
    numforge_result_destroy(first); numforge_result_destroy(second);
}

static void test_thousand_digits_independent_reference(void)
{
    /* Reuse the frozen independent Decimal/mpmath benchmark oracle, not NumForge. */
    FILE *file = fopen(NUMFORGE_ENGINE_REFERENCE_FILE, "r");
    char line[8192], *expected = NULL;
    TEST_ASSERT_NOT_NULL(file);
    while (fgets(line, sizeof(line), file) != NULL) {
        if (strncmp(line, "sqrt_ordinary_p1000\t", 20U) == 0) {
            expected = strrchr(line, '\t');
            if (expected != NULL) ++expected;
            break;
        }
    }
    fclose(file);
    TEST_ASSERT_NOT_NULL(expected);
    expected[strcspn(expected, "\r\n")] = '\0';
    NumForgeContext *context = numforge_context_create();
    NumForgeResult *result = NULL;
    char *actual = NULL;
    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_context_set_precision(context, 1000));
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_evaluate(&result, "sqrt(2)", context, NULL));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_to_string(numforge_result_decimal(result), &actual));
    TEST_ASSERT_EQUAL_STRING(expected, actual);
    free(actual);
    numforge_result_destroy(result); numforge_context_destroy(context);
}

static void test_shared_pipeline_parity(void)
{
    const char *inputs[] = {"1/3", "sqrt(2)", "sin(0.5)", "ln(2)", "sqrt(-2)", "complex(1/3;2/7)^2"};
    NumForgeContext *context = numforge_context_create();
    TEST_ASSERT_NOT_NULL(context);
    for (int mode = BIGDECIMAL_ROUND_TOWARD_ZERO; mode <= BIGDECIMAL_ROUND_HALF_EVEN; ++mode) {
        CalculatorContext original;
        calculator_context_init(&original);
        original.output_scale = -1;
        original.division_scale = 50;
        original.rounding = (BigDecimalRoundingMode)mode;
        TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_context_set_precision(context, 50));
        TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_context_set_rounding(context, original.rounding));
        for (size_t i = 0; i < sizeof(inputs)/sizeof(inputs[0]); ++i) {
            CalculatorValue value = {0};
            NumForgeResult *result = NULL;
            char *expected = NULL, *actual = NULL;
            TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute_value(inputs[i], &original, &value, NULL));
            TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_value_snapshot_text(&value, &expected));
            TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_evaluate(&result, inputs[i], context, NULL));
            TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_result_to_string(result, &actual));
            TEST_ASSERT_EQUAL_STRING(expected, actual);
            if (i == 4U) {
                TEST_ASSERT_EQUAL(NUMFORGE_RESULT_COMPLEX_DECIMAL, numforge_result_kind(result));
                TEST_ASSERT_NOT_NULL(numforge_result_complex_decimal(result));
                TEST_ASSERT_FALSE(numforge_result_is_exact(result));
            }
            free(expected); free(actual);
            calculator_value_destroy(&value); numforge_result_destroy(result);
        }
    }
    numforge_context_destroy(context);
}

static void test_diagnostics_and_atomic_errors(void)
{
    NumForgeResult *keeper = NULL;
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_evaluate(&keeper, "7", NULL, NULL));
    const char *inputs[] = {"1/0", "sqrt(", "x+1", "ans+1", "1+rand()", "qty(1;\"m\")", "sqrt(1;2)", "pi+)"};
    const NumForgeStatus statuses[] = {NUMFORGE_DIVISION_BY_ZERO, NUMFORGE_SYNTAX_ERROR,
        NUMFORGE_UNDEFINED_VARIABLE, NUMFORGE_UNSUPPORTED, NUMFORGE_UNSUPPORTED,
        NUMFORGE_UNSUPPORTED, NUMFORGE_ARGUMENT_COUNT, NUMFORGE_SYNTAX_ERROR};
    for (size_t i = 0; i < sizeof(inputs)/sizeof(inputs[0]); ++i) {
        NumForgeResult *output = keeper;
        NumForgeError error = {NUMFORGE_OK, 99U};
        TEST_ASSERT_EQUAL_MESSAGE(statuses[i], numforge_evaluate(&output, inputs[i], NULL, &error), inputs[i]);
        TEST_ASSERT_EQUAL(statuses[i], error.status);
        TEST_ASSERT_EQUAL_PTR(keeper, output);
    }
    NumForgeExpression *prepared = NULL;
    NumForgeError error;
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_expression_prepare(&prepared, "2+2", NULL, NULL));
    NumForgeExpression *output = prepared;
    TEST_ASSERT_EQUAL(NUMFORGE_SYNTAX_ERROR, numforge_expression_prepare(&output, "\xCF\x80+)", NULL, &error));
    TEST_ASSERT_EQUAL_PTR(prepared, output);
    TEST_ASSERT_EQUAL_UINT(3U, error.offset);
    char oversized[4098];
    memset(oversized, '1', sizeof(oversized)-1U); oversized[sizeof(oversized)-1U] = '\0';
    TEST_ASSERT_EQUAL(NUMFORGE_VALUE_TOO_LARGE, numforge_expression_prepare(&output, oversized, NULL, NULL));
    TEST_ASSERT_EQUAL_PTR(prepared, output);
    check_text(keeper, "7");
    numforge_expression_destroy(prepared); numforge_result_destroy(keeper);
}

static void test_nulls_and_context_validation(void)
{
    NumForgeResult *result = NULL;
    NumForgeExpression *expression = NULL;
    NumForgeContext *context = numforge_context_create();
    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_EQUAL(NUMFORGE_NULL_ARGUMENT, numforge_evaluate(NULL, "1", NULL, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_NULL_ARGUMENT, numforge_evaluate(&result, NULL, NULL, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_NULL_ARGUMENT, numforge_expression_prepare(NULL, "1", NULL, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_NULL_ARGUMENT, numforge_expression_prepare(&expression, NULL, NULL, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_NULL_ARGUMENT, numforge_expression_evaluate(&result, NULL, NULL, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_NULL_ARGUMENT, numforge_expression_evaluate(NULL, NULL, NULL, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_NULL_ARGUMENT, numforge_result_to_string(NULL, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_NULL_ARGUMENT, numforge_context_set_precision(NULL, 34));
    TEST_ASSERT_EQUAL(NUMFORGE_NULL_ARGUMENT, numforge_context_set_rounding(NULL, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(NUMFORGE_NULL_ARGUMENT, numforge_context_set_limits(NULL, 1, 1, 1));
    TEST_ASSERT_EQUAL(NUMFORGE_INVALID_ARGUMENT, numforge_context_set_precision(context, 0));
    TEST_ASSERT_EQUAL(NUMFORGE_INVALID_ARGUMENT, numforge_context_set_precision(context, 10001));
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_context_set_precision(context, 10000));
    TEST_ASSERT_EQUAL(NUMFORGE_INVALID_ARGUMENT, numforge_context_set_rounding(context, (BigDecimalRoundingMode)-1));
    TEST_ASSERT_EQUAL(NUMFORGE_INVALID_ARGUMENT, numforge_context_set_limits(context, UINT64_MAX, 1, 1));
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_context_set_precision(context, 1));
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_evaluate(&result, "1/3", context, NULL));
    check_text(result, "1/3");
    TEST_ASSERT_EQUAL(NUMFORGE_NULL_ARGUMENT, numforge_result_to_string(result, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_RESULT_INVALID, numforge_result_kind(NULL));
    TEST_ASSERT_FALSE(numforge_result_is_exact(NULL));
    TEST_ASSERT_NULL(numforge_result_integer(NULL)); TEST_ASSERT_NULL(numforge_result_rational(NULL));
    TEST_ASSERT_NULL(numforge_result_decimal(NULL)); TEST_ASSERT_NULL(numforge_result_complex_rational(NULL));
    TEST_ASSERT_NULL(numforge_result_complex_decimal(NULL));
    TEST_ASSERT_EQUAL_STRING("unknown status", numforge_status_to_string((NumForgeStatus)-1));
    for (int status = NUMFORGE_OK; status <= NUMFORGE_UNDEFINED_VARIABLE; ++status)
        TEST_ASSERT_TRUE(strcmp("unknown status", numforge_status_to_string((NumForgeStatus)status)) != 0);
    numforge_context_destroy(NULL); numforge_expression_destroy(NULL); numforge_result_destroy(NULL);
    numforge_context_destroy(context); numforge_result_destroy(result);
}

static void test_resource_limits_and_inheritance(void)
{
    NumForgeContext *context = numforge_context_create();
    NumForgeResult *result = NULL;
    NumForgeError error;
    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_context_set_limits(context, 0, SIZE_MAX, SIZE_MAX));
    TEST_ASSERT_EQUAL(NUMFORGE_TIME_LIMIT, numforge_evaluate(&result, "2+2", context, &error));
    TEST_ASSERT_EQUAL(NUMFORGE_TIME_LIMIT, error.status); TEST_ASSERT_NULL(result);
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_context_set_limits(context, 5000, 0, SIZE_MAX));
    TEST_ASSERT_EQUAL(NUMFORGE_MEMORY_LIMIT, numforge_evaluate(&result, "2+2", context, &error));
    TEST_ASSERT_EQUAL(NUMFORGE_MEMORY_LIMIT, error.status); TEST_ASSERT_NULL(result);
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_context_set_limits(context, 5000, SIZE_MAX, 0));
    TEST_ASSERT_EQUAL(NUMFORGE_MEMORY_LIMIT, numforge_evaluate(&result, "2+2", context, NULL));
    TEST_ASSERT_TRUE(numforge_budget_begin(5000, 0, SIZE_MAX));
    TEST_ASSERT_EQUAL(NUMFORGE_MEMORY_LIMIT, numforge_evaluate(&result, "2+2", NULL, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_MEMORY, numforge_budget_failure());
    TEST_ASSERT_FALSE(numforge_budget_begin(5000, SIZE_MAX, SIZE_MAX));
    numforge_budget_end();
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_evaluate(&result, "2+2", NULL, NULL));
    char marker = 'x', *text = &marker;
    TEST_ASSERT_TRUE(numforge_budget_begin(5000, 0, SIZE_MAX));
    TEST_ASSERT_EQUAL(NUMFORGE_MEMORY_LIMIT, numforge_result_to_string(result, &text));
    TEST_ASSERT_EQUAL_PTR(&marker, text);
    numforge_budget_end();
    check_text(result, "4");
    numforge_result_destroy(result); numforge_context_destroy(context);
}

static void test_allocation_failures_preserve_outputs(void)
{
    NumForgeResult *keeper = NULL;
    NumForgeExpression *prepared = NULL;
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_evaluate(&keeper, "7", NULL, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_expression_prepare(&prepared, "1/3+2/3", NULL, NULL));
    for (int operation = 0; operation < 4; ++operation) {
        bool completed = false;
        for (size_t failure = 1; failure < 4096U; ++failure) {
            NumForgeResult *result = keeper;
            NumForgeExpression *expression = prepared;
            char marker = 'x', *text = &marker;
            numforge_test_allocator_begin(failure);
            NumForgeStatus status = operation == 0 ? numforge_expression_prepare(&expression, "1/3+2/3", NULL, NULL) :
                operation == 1 ? numforge_expression_evaluate(&result, prepared, NULL, NULL) :
                operation == 2 ? numforge_evaluate(&result, "1/3+2/3", NULL, NULL) :
                numforge_result_to_string(keeper, &text);
            bool injected = numforge_test_allocator_did_fail();
            numforge_test_allocator_end();
            if (status == NUMFORGE_OK) {
                if (operation == 0) numforge_expression_destroy(expression);
                else if (operation == 3) { TEST_ASSERT_EQUAL_STRING("7", text); free(text); }
                else { check_text(result, "1"); numforge_result_destroy(result); }
            } else {
                TEST_ASSERT_EQUAL(NUMFORGE_OUT_OF_MEMORY, status);
                TEST_ASSERT_EQUAL_PTR(keeper, result);
                TEST_ASSERT_EQUAL_PTR(prepared, expression);
                TEST_ASSERT_EQUAL_PTR(&marker, text);
            }
            if (!injected) { TEST_ASSERT_EQUAL(NUMFORGE_OK, status); completed = true; break; }
        }
        TEST_ASSERT_TRUE(completed);
    }
    numforge_test_allocator_begin(1U);
    NumForgeContext *context = numforge_context_create();
    numforge_test_allocator_end();
    TEST_ASSERT_NULL(context);
    check_text(keeper, "7");
    numforge_expression_destroy(prepared); numforge_result_destroy(keeper);
}

static void test_cancellation_during_prepared_evaluation(void)
{
    NumForgeExpression *expression = NULL;
    NumForgeResult *keeper = NULL;
    NumForgeContext *context = numforge_context_create();
    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_context_set_precision(context, 1000));
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_expression_prepare(&expression, "1+sqrt(2)", NULL, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_evaluate(&keeper, "7", NULL, NULL));
    const size_t checkpoints[] = {1U, 16U, 128U};
    for (size_t i = 0U; i < sizeof(checkpoints)/sizeof(checkpoints[0]); ++i) {
        NumForgeResult *result = keeper;
        NumForgeError error = {NUMFORGE_OK, 99U};
        TEST_ASSERT_TRUE(numforge_budget_begin(5000U, SIZE_MAX, SIZE_MAX));
        numforge_test_budget_expire_after(checkpoints[i]);
        NumForgeStatus status = numforge_expression_evaluate(&result, expression, context, &error);
        NumForgeBudgetFailure failure = numforge_budget_failure();
        numforge_budget_end();
        TEST_ASSERT_EQUAL(NUMFORGE_TIME_LIMIT, status);
        TEST_ASSERT_EQUAL(NUMFORGE_TIME_LIMIT, error.status);
        TEST_ASSERT_EQUAL_UINT(0U, error.offset);
        TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_TIME, failure);
        TEST_ASSERT_EQUAL_PTR(keeper, result);
    }
    NumForgeResult *recovered = NULL;
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_expression_evaluate(&recovered, expression, context, NULL));
    TEST_ASSERT_NOT_NULL(numforge_result_decimal(recovered));
    check_text(keeper, "7");
    numforge_result_destroy(recovered); numforge_result_destroy(keeper);
    numforge_expression_destroy(expression); numforge_context_destroy(context);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_exact_typed_results);
    RUN_TEST(test_prepared_ownership_and_precision);
    RUN_TEST(test_thousand_digits_independent_reference);
    RUN_TEST(test_shared_pipeline_parity);
    RUN_TEST(test_diagnostics_and_atomic_errors);
    RUN_TEST(test_nulls_and_context_validation);
    RUN_TEST(test_resource_limits_and_inheritance);
    RUN_TEST(test_allocation_failures_preserve_outputs);
    RUN_TEST(test_cancellation_during_prepared_evaluation);
    return UNITY_END();
}
