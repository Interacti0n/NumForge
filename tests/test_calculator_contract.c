#include <stdlib.h>
#include <unity.h>

#include "calculator_internal.h"
#include "../src/bigdecimal/bigdecimal_internal.h"

/* ============================================================
   Calculator grammar and intermediate-rounding contract
   ============================================================ */

void setUp(void) {}
void tearDown(void) {}

static void assert_result(const char *input, const char *expected, CalculatorContext *context)
{
    CalculatorError error;
    char *text = NULL;
    CalculatorStatus status = calculator_compute(input, context, &text, &error);
    TEST_ASSERT_EQUAL_MESSAGE(CALCULATOR_OK, status, input);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(expected, text, input);
    free(text);
}

static void test_malformed_adjacent_numbers(void)
{
    static const struct { const char *input; CalculatorStatus status; size_t offset; } cases[] = {
        { "1.2.3", CALCULATOR_INVALID_TOKEN, 3 },
        { "1,2,3", CALCULATOR_INVALID_TOKEN, 3 },
        { "1.2,3", CALCULATOR_INVALID_TOKEN, 3 },
        { "1..2", CALCULATOR_INVALID_TOKEN, 2 },
        { "1E3.4", CALCULATOR_INVALID_TOKEN, 3 },
        { "2 3", CALCULATOR_SYNTAX_ERROR, 2 },
        { "2\t.3", CALCULATOR_SYNTAX_ERROR, 2 },
        { "2^3 4", CALCULATOR_SYNTAX_ERROR, 4 },
        { "(2 3)", CALCULATOR_SYNTAX_ERROR, 3 },
        { "π 2 3", CALCULATOR_SYNTAX_ERROR, 5 }
    };
    CalculatorContext context;
    calculator_context_init(&context);
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        CalculatorError error;
        char *text = NULL;
        TEST_ASSERT_EQUAL_MESSAGE(cases[i].status,
            calculator_compute(cases[i].input, &context, &text, &error), cases[i].input);
        TEST_ASSERT_NULL(text);
        TEST_ASSERT_EQUAL_UINT(cases[i].offset, error.offset);
    }
    assert_result("2+3", "5", &context);
}

static void test_implicit_products_and_decimal_separators(void)
{
    CalculatorContext context;
    calculator_context_init(&context);
    assert_result("2(2+2)", "8", &context);
    assert_result("(2+2)3", "12", &context);
    assert_result("(2)(3)", "6", &context);
    assert_result("3!2", "12", &context);
    assert_result("2²3", "12", &context);
    assert_result("2π-2*π", "0", &context);
    assert_result("πe-π*e", "0", &context);
    assert_result("1e3-1*e*3", "0", &context);
    assert_result("1E3", "1000", &context);
    assert_result("1E+3", "1000", &context);
    assert_result("1,5+.25+1.", "2.75", &context);
    assert_result("6/2(1+2)", "9", &context);
}

static void test_intermediate_rounding_is_not_final_rounding(void)
{
    CalculatorContext context;
    calculator_context_init(&context);
    assert_result("(1/3)*3-1", "-1E-34", &context);
    assert_result("1E34/1+1-1E34", "1", &context);
    assert_result("(1E34+1)/1-1E34", "1", &context);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_context_set_output_scale(&context, 40));
    assert_result("(1E34+1)/1-1E34", "1", &context);
    assert_result("(1/3)*3-1", "-1E-44", &context);
    TEST_ASSERT_EQUAL(CALCULATOR_OK,
        calculator_context_set_output_scale(&context, CALCULATOR_UNLIMITED_OUTPUT_SCALE));
    assert_result("(1/3)*3-1", "-1E-34", &context);
    assert_result("1/8", "0.125", &context);
}

static void test_finite_division_preserves_exact_intermediates(void)
{
    CalculatorContext context;
    calculator_context_init(&context);
    assert_result("((1E80+1)/8)*8-1E80", "1", &context);
    assert_result("((1E80+1)/-2)*-2-1E80", "1", &context);
    assert_result("3/6", "0.5", &context);
    assert_result("7/28", "0.25", &context);
    assert_result("1/0.008", "125", &context);
    assert_result("0/7", "0", &context);
    assert_result("1E-9223372036854775807/2E-1", "5E-9223372036854775807", &context);
    TEST_ASSERT_EQUAL(CALCULATOR_OK,
        calculator_context_set_output_scale(&context, CALCULATOR_UNLIMITED_OUTPUT_SCALE));
    context.division_scale = 2;
    assert_result("1/128", "0.0078125", &context);
    assert_result("1/625", "0.0016", &context);
    assert_result("1/40", "0.025", &context);
    assert_result("1/6", "0.17", &context);
    assert_result("1/7", "0.14", &context);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_context_set_output_scale(&context, 2));
    assert_result("1/8", "0.12", &context);
}

static void test_exact_division_aliasing_and_scale_failure(void)
{
    BigDecimal *a = bigdecimal_create();
    BigDecimal *b = bigdecimal_create();
    char *text = NULL;
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(a, "7"));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(b, "28"));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,
        bigdecimal_div_exact_or_significant(b, a, b, 1, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_to_string(b, &text));
    TEST_ASSERT_EQUAL_STRING("0.25", text);
    free(text);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,
        bigdecimal_div_exact_or_significant(a, a, b, 1, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_to_string(a, &text));
    TEST_ASSERT_EQUAL_STRING("28", text);
    free(text);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(a, "1e-9223372036854775807"));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(b, "2"));
    TEST_ASSERT_EQUAL(BIGDECIMAL_SCALE_OVERFLOW,
        bigdecimal_div_exact_or_significant(b, a, b, 34, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_to_string(b, &text));
    TEST_ASSERT_EQUAL_STRING("2", text);
    free(text);
    bigdecimal_destroy(a);
    bigdecimal_destroy(b);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_malformed_adjacent_numbers);
    RUN_TEST(test_implicit_products_and_decimal_separators);
    RUN_TEST(test_intermediate_rounding_is_not_final_rounding);
    RUN_TEST(test_finite_division_preserves_exact_intermediates);
    RUN_TEST(test_exact_division_aliasing_and_scale_failure);
    return UNITY_END();
}
