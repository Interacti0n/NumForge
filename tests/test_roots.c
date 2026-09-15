#include <stdlib.h>
#include <unity.h>
#include <numforge/bigdecimal.h>
#include "calculator_internal.h"

/* Real-root domains, exactness, scales and rounding (separate from grammar). */
void setUp(void) {}
void tearDown(void) {}

static void test_root_expressions(void)
{
    static const struct { const char *input; const char *expected; } cases[] = {
        { "sqrt(0)", "0" }, { "cbrt(-0)", "0" }, { "sqrt(1)", "1" },
        { "√(4)", "2" }, { "2sqrt(abs(-4))", "4" },
        { "sqrt(0.0004)", "0.02" }, { "cbrt(-0.125)", "-0.5" },
        { "root(-32;5)", "-2" }, { "root(-2.5;1)", "-2.5" },
        { "root(16;4.00)", "2" }, { "root(1;10000)", "1" }, { "root(0;10000)", "0" },
        { "root(-1;9999)", "-1" }, { "sqrt(2)", "1.4142135624" },
        { "cbrt(2)", "1.2599210499" }, { "cbrt(-2)", "-1.2599210499" },
        { "root(2;5)", "1.148698355" }, { "sqrt(1E100000)", "1E+50000" },
        { "sqrt(2E-100000)", "1.4142135624E-50000" },
        { "cbrt(-2E99999)", "-1.2599210499E+33333" },
        { "sqrt((1E80+1)^2)-(1E80+1)", "0" },
        { "root((1E50+3)^3;3)-(1E50+3)", "0" },
        { "sqrt(1E9223372036854775806)", "1E+4611686018427387903" },
        { "sqrt(1E-9223372036854775806)", "1E-4611686018427387903" }
    };
    CalculatorContext context;
    CalculatorError error;
    calculator_context_init(&context);
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        char *text = NULL;
        TEST_ASSERT_EQUAL_MESSAGE(CALCULATOR_OK, calculator_compute(cases[i].input, &context, &text, &error), cases[i].input);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(cases[i].expected, text, cases[i].input);
        free(text);
    }
    static const char *const invalid[] = {
        "sqrt(-1)", "root(-16;4)", "root(2;0)", "root(2;-3)",
        "root(2;1.5)", "root(0;0)", "root(1;1E-100000)"
    };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
    {
        char *text = NULL;
        TEST_ASSERT_EQUAL_MESSAGE(CALCULATOR_INVALID_ARGUMENT, calculator_compute(invalid[i], &context, &text, &error), invalid[i]);
        TEST_ASSERT_NULL(text);
    }
    char *text = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_TOO_LARGE, calculator_compute("root(1;10001)", &context, &text, &error));
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_TOO_LARGE, calculator_compute("root(1;1E100000)", &context, &text, &error));
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_TOO_LARGE, calculator_compute("root(2;10000)", &context, &text, &error));
    TEST_ASSERT_EQUAL(CALCULATOR_DIVISION_BY_ZERO, calculator_compute("root(2;1/0)", &context, &text, &error));
    TEST_ASSERT_EQUAL_UINT(8, error.offset);
    TEST_ASSERT_NULL(text);
}

static void assert_root(const char *input, uint32_t degree, int64_t precision,
    BigDecimalRoundingMode rounding, const char *expected)
{
    BigDecimal *value = bigdecimal_create(), *reference = bigdecimal_create();
    TEST_ASSERT_NOT_NULL(value); TEST_ASSERT_NOT_NULL(reference);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, input));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(reference, expected));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_root(value, value, degree, precision, rounding));
    int comparison = 1;
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_compare(&comparison, value, reference));
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, comparison, input);
    bigdecimal_destroy(value); bigdecimal_destroy(reference);
}

static void test_rounding_and_extreme_scales(void)
{
    static const char *const positive[] = { "1.41", "1.42", "1.41", "1.42", "1.41", "1.41" };
    static const char *const negative[] = { "-1.25", "-1.26", "-1.26", "-1.25", "-1.26", "-1.26" };
    for (int mode = 0; mode < 6; mode++)
    {
        assert_root("2", 2, 3, (BigDecimalRoundingMode)mode, positive[mode]);
        assert_root("-2", 3, 3, (BigDecimalRoundingMode)mode, negative[mode]);
    }
    assert_root("2.102499999999", 2, 2, BIGDECIMAL_ROUND_HALF_EVEN, "1.4");
    assert_root("2.102500000001", 2, 2, BIGDECIMAL_ROUND_HALF_EVEN, "1.5");
    assert_root("2.1025", 2, 2, BIGDECIMAL_ROUND_HALF_EVEN, "1.45"); /* exact stays exact */
    assert_root("99.999999999", 2, 3, BIGDECIMAL_ROUND_HALF_EVEN, "10");
    assert_root("1E9223372036854775807", 2, 3, BIGDECIMAL_ROUND_HALF_EVEN, "3.16E4611686018427387903");
    assert_root("1E-9223372036854775807", 2, 3, BIGDECIMAL_ROUND_HALF_EVEN, "3.16E-4611686018427387904");
    assert_root("2E9223372036854775807", 3, 3, BIGDECIMAL_ROUND_HALF_EVEN, "2.71E3074457345618258602");
    assert_root("2", 2, 40, BIGDECIMAL_ROUND_HALF_EVEN, "1.41421356237309504880168872420969807857");
}

static void test_root_failure_preserves_output(void)
{
    BigDecimal *value = bigdecimal_create();
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, "-2"));
    TEST_ASSERT_EQUAL(BIGDECIMAL_INVALID_ARGUMENT, bigdecimal_root(value, value, 2, 34, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_INVALID_ARGUMENT, bigdecimal_root(value, value, 0, 34, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_INVALID_ARGUMENT, bigdecimal_root(value, value, 3, 0, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_root(NULL, value, 3, 34, BIGDECIMAL_ROUND_HALF_EVEN));
    char *text = NULL;
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_to_string(value, &text));
    TEST_ASSERT_EQUAL_STRING("-2", text);
    free(text); bigdecimal_destroy(value);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_root_expressions);
    RUN_TEST(test_rounding_and_extreme_scales);
    RUN_TEST(test_root_failure_preserves_output);
    return UNITY_END();
}
