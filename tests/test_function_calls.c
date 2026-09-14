#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "expression_internal.h"
#include "calculator_internal.h"

/* Function grammar, aliases, limits and constant/scientific token boundaries. */
void setUp(void) {}
void tearDown(void) {}

static void test_registered_calls_and_arity(void)
{
    static const char *const inputs[] = {
        "abs(1)", "sign(1)", "min(1;2;3)", "max(1;2)", "gcd(12;18)",
        "lcm(2;3)", "mod(3;2)", "factorial(3)", "isqrt(4)", "pow(2;3)",
        "sqrt(4)", "cbrt(-8)", "root(8;3)", "exp(2)", "ln(e)", "log(10)",
        "log(8;2)", "sin(1)", "cos(1)", "tan(1)", "asin(1)", "acos(1)",
        "atan(1)", "radians(90)", "degrees(1)", "sqrt(abs(-4))", "√(4)"
    };
    for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++)
    {
        CalculatorExpression *expression = NULL;
        CalculatorError error;
        TEST_ASSERT_EQUAL_MESSAGE(CALCULATOR_OK, calculator_parse(inputs[i], &expression, &error), inputs[i]);
        TEST_ASSERT_EQUAL(CALCULATOR_EXPRESSION_CALL, expression->type);
        calculator_expression_destroy(expression);
    }
}

static void test_call_errors(void)
{
    static const struct { const char *input; CalculatorStatus status; size_t offset; } cases[] = {
        { "log()", CALCULATOR_ARGUMENT_COUNT, 0 },
        { "log(1;2;3)", CALCULATOR_ARGUMENT_COUNT, 0 },
        { "atan(1;2)", CALCULATOR_ARGUMENT_COUNT, 0 },
        { "min(1)", CALCULATOR_ARGUMENT_COUNT, 0 },
        { "gcd(12,18)", CALCULATOR_ARGUMENT_COUNT, 0 },
        { "2+sin()", CALCULATOR_ARGUMENT_COUNT, 2 },
        { "sin 2", CALCULATOR_SYNTAX_ERROR, 4 },
        { "log2(8)", CALCULATOR_SYNTAX_ERROR, 3 },
        { "sqrt(1;)", CALCULATOR_SYNTAX_ERROR, 7 },
        { "log(;2)", CALCULATOR_SYNTAX_ERROR, 4 },
        { "log(1;;2)", CALCULATOR_SYNTAX_ERROR, 6 },
        { "log(1", CALCULATOR_SYNTAX_ERROR, 5 },
        { "1;2", CALCULATOR_SYNTAX_ERROR, 1 },
        { "√4", CALCULATOR_SYNTAX_ERROR, 3 },
        { "sni(2)", CALCULATOR_INVALID_TOKEN, 0 },
        { "SIN(2)", CALCULATOR_INVALID_TOKEN, 0 },
        { "pi", CALCULATOR_INVALID_TOKEN, 0 },
        { "phi", CALCULATOR_INVALID_TOKEN, 0 },
        { "ee", CALCULATOR_INVALID_TOKEN, 0 },
        { "expfoo(2)", CALCULATOR_INVALID_TOKEN, 0 }
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        CalculatorExpression *expression = NULL;
        CalculatorError error;
        TEST_ASSERT_EQUAL_MESSAGE(cases[i].status, calculator_parse(cases[i].input, &expression, &error), cases[i].input);
        TEST_ASSERT_NULL(expression);
        TEST_ASSERT_EQUAL_UINT_MESSAGE(cases[i].offset, error.offset, cases[i].input);
    }
}

static void test_evaluation_and_implicit_products(void)
{
    static const struct { const char *input; const char *expected; } cases[] = {
        { "pow(1,5;3)", "3.375" }, { "factorial(5)", "120" },
        { "pow(2;pow(3;2))", "512" }, { "2pow(2;3)factorial(3)", "96" },
        { "pow(2;3)²", "64" }, { "factorial(3)^2", "36" },
        { "1e3-1*e*3", "0" }, { "1E3", "1000" },
        { "πe-π*e", "0" }, { "e(2+2)-4*e", "0" }, { "2(2+2)", "8" },
        { "pow(0;0)", "1" }
    };
    CalculatorContext context;
    CalculatorError error;
    calculator_context_init(&context);
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        char *text = NULL;
        TEST_ASSERT_EQUAL_MESSAGE(CALCULATOR_OK, calculator_compute(cases[i].input, &context, &text, &error), cases[i].input);
        TEST_ASSERT_EQUAL_STRING(cases[i].expected, text);
        free(text);
    }
    static const char *const pending[] = { "exp(2)", "2sin(1)", "sqrt(abs(-4))", "log(1/0)", "√(4)" };
    for (size_t i = 0; i < sizeof(pending) / sizeof(pending[0]); i++)
    {
        char *text = NULL;
        TEST_ASSERT_EQUAL(CALCULATOR_NOT_IMPLEMENTED, calculator_compute(pending[i], &context, &text, &error));
        TEST_ASSERT_NULL(text);
    }
    char *text = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT, calculator_compute("pow(2;-1)", &context, &text, &error));
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT, calculator_compute("factorial(1.5)", &context, &text, &error));
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_TOO_LARGE, calculator_compute("factorial(10001)", &context, &text, &error));
    TEST_ASSERT_NULL(text);
}

static void test_call_limits(void)
{
    char input[2048];
    CalculatorExpression *expression = NULL;
    CalculatorError error;
    size_t used = 0;
    for (size_t i = 0; i < CALCULATOR_MAX_EXPRESSION_DEPTH; i++)
    {
        memcpy(input + used, "sin(", 4); used += 4;
    }
    input[used++] = '1';
    for (size_t i = 0; i < CALCULATOR_MAX_EXPRESSION_DEPTH; i++) input[used++] = ')';
    input[used] = '\0';
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_TOO_LARGE, calculator_parse(input, &expression, &error));
    TEST_ASSERT_NULL(expression);
    memcpy(input, "min(", 4); used = 4;
    for (size_t i = 0; i < CALCULATOR_MAX_CALL_ARGUMENTS; i++)
    {
        input[used++] = '1'; input[used++] = ';';
    }
    input[used - 1] = ')'; input[used] = '\0';
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_parse(input, &expression, &error));
    calculator_expression_destroy(expression); expression = NULL;
    input[used - 1] = ';'; input[used++] = '1'; input[used++] = ')'; input[used] = '\0';
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_TOO_LARGE, calculator_parse(input, &expression, &error));
    TEST_ASSERT_NULL(expression);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_registered_calls_and_arity);
    RUN_TEST(test_call_errors);
    RUN_TEST(test_evaluation_and_implicit_products);
    RUN_TEST(test_call_limits);
    return UNITY_END();
}
