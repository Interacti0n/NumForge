#include <numforge/engine.h>
#include <numforge/runtime.h>
#include "numforge_alloc.h"
#include <unity.h>
#include <stdlib.h>
#include <string.h>

static NumForgeContext *context;
static NumForgeExpression *expression;
static NumForgeResult *argument[3], *result;
void setUp(void) { context = numforge_context_create(); TEST_ASSERT_NOT_NULL(context); }
void tearDown(void)
{
    numforge_test_allocator_end(); numforge_test_budget_expire_after(0U); numforge_budget_end();
    for (size_t i = 0U; i < 3U; ++i) { numforge_result_destroy(argument[i]); argument[i] = NULL; }
    numforge_result_destroy(result); result = NULL;
    numforge_expression_destroy(expression); expression = NULL;
    numforge_context_destroy(context); context = NULL;
}
static void input(size_t index, const char *text)
{
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_evaluate(&argument[index], text, NULL, NULL));
}
static void output(const char *expected)
{
    char *text = NULL;
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_result_to_string(result, &text));
    TEST_ASSERT_EQUAL_STRING(expected, text); free(text);
}
static void test_declared_parameters_and_reuse(void)
{
    char first[] = "x", second[] = "Y", source[] = "x*x+Y";
    const char *names[] = {first, second};
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_expression_prepare_parameters(&expression, source, names, 2U, NULL, NULL));
    first[0] = 'a'; second[0] = 'b'; memset(source, 'z', sizeof(source)-1U);
    TEST_ASSERT_EQUAL_UINT(2U, numforge_expression_parameter_count(expression));
    TEST_ASSERT_EQUAL_STRING("x", numforge_expression_parameter_name(expression, 0U));
    TEST_ASSERT_EQUAL_STRING("Y", numforge_expression_parameter_name(expression, 1U));
    TEST_ASSERT_NULL(numforge_expression_parameter_name(expression, 2U));
    TEST_ASSERT_NULL(numforge_expression_parameter_name(NULL, 0U));
    TEST_ASSERT_EQUAL_UINT(0U, numforge_expression_parameter_count(NULL));
    input(0, "1/3"); input(1, "1/9");
    const NumForgeResult *values[] = {argument[0], argument[1]};
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_expression_evaluate_arguments(&result, expression, values, 2U, NULL, NULL));
    TEST_ASSERT_TRUE(numforge_result_is_exact(result)); output("2/9");
    numforge_result_destroy(result); result = NULL;
    numforge_result_destroy(argument[0]); argument[0] = NULL; input(0, "2"); values[0] = argument[0];
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_expression_evaluate_arguments(&result, expression, values, 2U, NULL, NULL));
    output("37/9");
}
static void test_validation_and_unchanged_output(void)
{
    const char *name[] = {"x"};
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_expression_prepare_parameters(&expression, "1/x", name, 1U, NULL, NULL));
    NumForgeExpression *keeper = expression;
    const char *invalid[] = {"", "sin", "i", "ans", "e", "x1", "x_y", "abcdefghijklmnopqrstuvwxyzabcdef"};
    for (size_t i = 0U; i < sizeof(invalid)/sizeof(invalid[0]); ++i) {
        const char *names[] = {invalid[i]};
        TEST_ASSERT_EQUAL(NUMFORGE_INVALID_ARGUMENT, numforge_expression_prepare_parameters(&expression, "1", names, 1U, NULL, NULL));
        TEST_ASSERT_EQUAL_PTR(keeper, expression);
    }
    const char *duplicate[] = {"x", "x"}, *missing[] = {NULL};
    TEST_ASSERT_EQUAL(NUMFORGE_INVALID_ARGUMENT, numforge_expression_prepare_parameters(&expression, "x", duplicate, 2U, NULL, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_NULL_ARGUMENT, numforge_expression_prepare_parameters(&expression, "x", missing, 1U, NULL, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_NULL_ARGUMENT, numforge_expression_prepare_parameters(&expression, "x", NULL, 1U, NULL, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_INVALID_ARGUMENT, numforge_expression_prepare_parameters(&expression, "x", name, 33U, NULL, NULL));
    NumForgeError error;
    TEST_ASSERT_EQUAL(NUMFORGE_UNDEFINED_VARIABLE, numforge_expression_prepare_parameters(&expression, "x+y", name, 1U, NULL, &error));
    TEST_ASSERT_EQUAL_UINT(2U, error.offset);
    TEST_ASSERT_EQUAL(NUMFORGE_UNSUPPORTED, numforge_expression_prepare_parameters(&expression, "x+rand()", name, 1U, NULL, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_ARGUMENT_COUNT, numforge_expression_evaluate(&result, expression, NULL, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_NULL_ARGUMENT, numforge_expression_evaluate_arguments(&result, expression, NULL, 1U, NULL, NULL));
    const NumForgeResult *values[] = {NULL};
    TEST_ASSERT_EQUAL(NUMFORGE_NULL_ARGUMENT, numforge_expression_evaluate_arguments(&result, expression, values, 1U, NULL, NULL));
    input(0, "0"); input(1, "7"); result = argument[1]; argument[1] = NULL;
    values[0] = argument[0]; NumForgeResult *saved = result;
    TEST_ASSERT_EQUAL(NUMFORGE_DIVISION_BY_ZERO, numforge_expression_evaluate_arguments(&result, expression, values, 1U, NULL, &error));
    TEST_ASSERT_EQUAL_PTR(saved, result); output("7");
    numforge_result_destroy(argument[0]); argument[0] = NULL; input(0, "2"); values[0] = argument[0];
    numforge_result_destroy(result); result = NULL;
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_expression_evaluate_arguments(&result, expression, values, 1U, NULL, NULL)); output("1/2");
}
static void test_direct_arguments_and_provenance(void)
{
    BigInt *integer = bigint_create(); BigRational *rational = bigrational_create();
    BigDecimal *decimal = bigdecimal_create(); BigComplex *complex = bigcomplex_create();
    BigRationalComplex *exact_complex = bigrationalcomplex_create();
    TEST_ASSERT_NOT_NULL(integer); TEST_ASSERT_NOT_NULL(rational); TEST_ASSERT_NOT_NULL(decimal);
    TEST_ASSERT_NOT_NULL(complex); TEST_ASSERT_NOT_NULL(exact_complex);
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(integer, "123456789012345678901234567890"));
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_result_from_integer(&argument[0], integer, NULL, NULL));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_from_bigint(rational, integer));
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_result_from_rational(&argument[1], rational, NULL, NULL));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK, bigrationalcomplex_set_parts(exact_complex, rational, rational));
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_result_from_complex_rational(&argument[2], exact_complex, NULL, NULL));
    bigint_destroy(integer); bigrational_destroy(rational); bigrationalcomplex_destroy(exact_complex);
    const char *names[] = {"x", "y", "z"};
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_expression_prepare_parameters(&expression, "z+x-y", names, 3U, NULL, NULL));
    const NumForgeResult *values[] = {argument[0], argument[1], argument[2]};
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_expression_evaluate_arguments(&result, expression, values, 3U, NULL, NULL));
    TEST_ASSERT_TRUE(numforge_result_is_exact(result));
    TEST_ASSERT_NOT_NULL(numforge_result_complex_rational(result));
    numforge_result_destroy(result); result = NULL;
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(decimal, "0.3"));
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_result_from_decimal(&result, decimal, NULL, NULL));
    TEST_ASSERT_FALSE(numforge_result_is_exact(result));
    numforge_result_destroy(result); result = NULL;
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK, bigcomplex_set_strings(complex, "0.3", "0.7"));
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_result_from_complex_decimal(&result, complex, NULL, NULL));
    bigdecimal_destroy(decimal); bigcomplex_destroy(complex);
    TEST_ASSERT_FALSE(numforge_result_is_exact(result)); TEST_ASSERT_NOT_NULL(numforge_result_complex_decimal(result));
}
static void test_aliases_and_precision(void)
{
    const char *names[] = {"x", "y"};
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_expression_prepare_parameters(&expression, "x+y", names, 2U, NULL, NULL));
    input(0, "1/3"); const NumForgeResult *values[] = {argument[0], argument[0]};
    NumForgeResult *original = argument[0];
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_expression_evaluate_arguments(&argument[0], expression, values, 2U, NULL, NULL));
    numforge_result_destroy(original);
    result = argument[0]; argument[0] = NULL; output("2/3");
    numforge_result_destroy(result); result = NULL; numforge_expression_destroy(expression); expression = NULL;
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_expression_prepare_parameters(&expression, "sqrt(x)", names, 1U, NULL, NULL));
    input(0, "2"); values[0] = argument[0];
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_context_set_precision(context, 1000));
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_expression_evaluate_arguments(&result, expression, values, 1U, context, NULL));
    char *text = NULL;
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_to_string(numforge_result_decimal(result), &text));
    TEST_ASSERT_EQUAL_UINT(1001U, strlen(text));
    TEST_ASSERT_TRUE(strncmp(text, "1.414213562373095048801688724209698", 35U) == 0); free(text);
}
static NumForgeStatus copy_result(NumForgeResult **destination, const NumForgeResult *source, int kind)
{
    switch (kind) {
        case 0: return numforge_result_from_integer(destination, numforge_result_integer(source), NULL, NULL);
        case 1: return numforge_result_from_rational(destination, numforge_result_rational(source), NULL, NULL);
        case 2: return numforge_result_from_decimal(destination, numforge_result_decimal(source), NULL, NULL);
        case 3: return numforge_result_from_complex_rational(destination, numforge_result_complex_rational(source), NULL, NULL);
        default: return numforge_result_from_complex_decimal(destination, numforge_result_complex_decimal(source), NULL, NULL);
    }
}
static void test_allocation_and_deadline_failures(void)
{
    const char *names[] = {"x"};
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_expression_prepare_parameters(&expression, "x+x", names, 1U, NULL, NULL));
    input(0, "1/3"); const NumForgeResult *values[] = {argument[0]};
    bool finished = false;
    for (size_t failure = 1U; failure < 4096U; ++failure) {
        numforge_test_allocator_begin(failure);
        NumForgeStatus status = numforge_expression_evaluate_arguments(&result, expression, values, 1U, NULL, NULL);
        bool injected = numforge_test_allocator_did_fail(); numforge_test_allocator_end();
        if (status == NUMFORGE_OK) { output("2/3"); numforge_result_destroy(result); result = NULL; }
        else { TEST_ASSERT_EQUAL(NUMFORGE_OUT_OF_MEMORY, status); TEST_ASSERT_NULL(result); }
        if (!injected) { finished = true; break; }
    }
    TEST_ASSERT_TRUE(finished);
    TEST_ASSERT_TRUE(numforge_budget_begin(5000U, SIZE_MAX, SIZE_MAX));
    numforge_test_budget_expire_after(16U);
    TEST_ASSERT_EQUAL(NUMFORGE_TIME_LIMIT, numforge_expression_evaluate_arguments(&result, expression, values, 1U, NULL, NULL));
    numforge_budget_end();
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_expression_evaluate_arguments(&result, expression, values, 1U, NULL, NULL)); output("2/3");
}
static void test_constructor_allocation_failures_and_nulls(void)
{
    const char *inputs[] = {"7", "1/3", "sqrt(2)", "i", "sqrt(-2)"};
    for (int kind = 0; kind < 5; ++kind) {
        input(0, inputs[kind]); bool finished = false;
        TEST_ASSERT_EQUAL(NUMFORGE_NULL_ARGUMENT, copy_result(NULL, argument[0], kind));
        TEST_ASSERT_EQUAL(NUMFORGE_NULL_ARGUMENT, copy_result(&result, NULL, kind));
        for (size_t failure = 1U; failure < 4096U; ++failure) {
            NumForgeResult *candidate = argument[0];
            numforge_test_allocator_begin(failure);
            NumForgeStatus status = copy_result(&candidate, argument[0], kind);
            bool injected = numforge_test_allocator_did_fail(); numforge_test_allocator_end();
            if (status == NUMFORGE_OK) {
                TEST_ASSERT_EQUAL(numforge_result_kind(argument[0]), numforge_result_kind(candidate));
                numforge_result_destroy(candidate);
            } else { TEST_ASSERT_EQUAL(NUMFORGE_OUT_OF_MEMORY, status); TEST_ASSERT_EQUAL_PTR(argument[0], candidate); }
            if (!injected) { finished = true; break; }
        }
        TEST_ASSERT_TRUE(finished); numforge_result_destroy(argument[0]); argument[0] = NULL;
    }
}
static void test_parameter_boundaries_and_approximate_input(void)
{
    char storage[32][3]; const char *names[32];
    const NumForgeResult *values[32];
    input(0, "1/3");
    for (size_t i = 0U; i < 32U; ++i) {
        storage[i][0] = 'p'; storage[i][1] = (char)(i < 26U ? 'A'+i : 'a'+i-26U); storage[i][2] = '\0';
        names[i] = storage[i]; values[i] = argument[0];
    }
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_expression_prepare_parameters(&expression, "pA+pf", names, 32U, NULL, NULL));
    TEST_ASSERT_EQUAL_UINT(32U, numforge_expression_parameter_count(expression));
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_expression_evaluate_arguments(&result, expression, values, 32U, NULL, NULL)); output("2/3");
    numforge_result_destroy(result); result = NULL; numforge_expression_destroy(expression); expression = NULL;
    const char *long_name[] = {"abcdefghijklmnopqrstuvwxyzabcde"};
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_expression_prepare_parameters(&expression, long_name[0], long_name, 1U, NULL, NULL));
    BigDecimal *value = bigdecimal_create(); TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, "0.3"));
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_result_from_decimal(&argument[1], value, NULL, NULL)); bigdecimal_destroy(value);
    values[0] = argument[1]; TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_context_set_precision(context, 1000));
    TEST_ASSERT_EQUAL(NUMFORGE_OK, numforge_expression_evaluate_arguments(&result, expression, values, 1U, context, NULL));
    TEST_ASSERT_FALSE(numforge_result_is_exact(result)); TEST_ASSERT_NOT_NULL(numforge_result_decimal(result));
}
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_declared_parameters_and_reuse);
    RUN_TEST(test_validation_and_unchanged_output);
    RUN_TEST(test_direct_arguments_and_provenance);
    RUN_TEST(test_aliases_and_precision);
    RUN_TEST(test_allocation_and_deadline_failures);
    RUN_TEST(test_constructor_allocation_failures_and_nulls);
    RUN_TEST(test_parameter_boundaries_and_approximate_input);
    return UNITY_END();
}
