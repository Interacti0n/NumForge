#include <stdlib.h>
#include <string.h>

#include <unity.h>

#include <numforge/bigdecimal.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Stored and dynamically calculated mathematical constants, precision,
    rounding, aliasing, domains, and compatibility of the legacy factory.
------------------------------------------------------------------------------------------------------------------------------
*/

void setUp(void)
{
}

void tearDown(void)
{
}

static BigDecimal *make_decimal(const char *text)
{
    BigDecimal *value = bigdecimal_create();

    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, text));
    return value;
}

static char *decimal_text(const BigDecimal *value)
{
    char *text = NULL;

    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_to_string(value, &text));
    TEST_ASSERT_NOT_NULL(text);
    return text;
}

static void assert_constant_text(
    BigDecimalConstant constant,
    int64_t digits,
    BigDecimalRoundingMode rounding,
    const char *expected
)
{
    BigDecimal *value = bigdecimal_create();
    char *text;

    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,
                      bigdecimal_set_constant_significant(value, constant, digits, rounding));
    text = decimal_text(value);
    TEST_ASSERT_EQUAL_STRING(expected, text);
    free(text);
    bigdecimal_destroy(value);
}

static void test_stored_precision_and_rounding(void)
{
    assert_constant_text(
        BIGDECIMAL_CONSTANT_PI, 10, BIGDECIMAL_ROUND_HALF_EVEN, "3.141592654");
    assert_constant_text(
        BIGDECIMAL_CONSTANT_E, 10, BIGDECIMAL_ROUND_HALF_EVEN, "2.718281828");
    assert_constant_text(
        BIGDECIMAL_CONSTANT_PHI, 10, BIGDECIMAL_ROUND_HALF_EVEN, "1.618033989");
    assert_constant_text(
        BIGDECIMAL_CONSTANT_PI, 5, BIGDECIMAL_ROUND_FLOOR, "3.1415");
    assert_constant_text(
        BIGDECIMAL_CONSTANT_PI, 5, BIGDECIMAL_ROUND_CEILING, "3.1416");
}

static void test_dynamic_values_extend_stored_prefixes(void)
{
    static const BigDecimalConstant constants[] = {
        BIGDECIMAL_CONSTANT_PI,
        BIGDECIMAL_CONSTANT_E,
        BIGDECIMAL_CONSTANT_PHI
    };

    for (size_t index = 0U; index < sizeof(constants) / sizeof(constants[0]); index++)
    {
        BigDecimal *stored = bigdecimal_create();
        BigDecimal *dynamic = bigdecimal_create();
        char *stored_text;
        char *dynamic_text;

        TEST_ASSERT_NOT_NULL(stored);
        TEST_ASSERT_NOT_NULL(dynamic);
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_constant(stored, constants[index]));
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK,
                          bigdecimal_set_constant_significant(
                              dynamic, constants[index], 520, BIGDECIMAL_ROUND_HALF_EVEN));
        stored_text = decimal_text(stored);
        dynamic_text = decimal_text(dynamic);
        TEST_ASSERT_GREATER_THAN(strlen(stored_text), strlen(dynamic_text));
        TEST_ASSERT_EQUAL_INT_MESSAGE(
            0,
            strncmp(stored_text, dynamic_text, strlen(stored_text)),
            index == 0U ? "pi" : index == 1U ? "e" : "phi");
        free(stored_text);
        free(dynamic_text);
        bigdecimal_destroy(stored);
        bigdecimal_destroy(dynamic);
    }
}

static void test_aliasing_and_failure_safety(void)
{
    BigDecimal *value = make_decimal("42");

    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,
                      bigdecimal_set_constant_significant(
                          value, BIGDECIMAL_CONSTANT_PI, 40, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_INVALID_ARGUMENT,
                      bigdecimal_set_constant_significant(
                          value, BIGDECIMAL_CONSTANT_PI, 0, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_INVALID_ARGUMENT,
                      bigdecimal_set_constant_significant(
                          value, (BigDecimalConstant)99, 20, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_INVALID_ARGUMENT,
                      bigdecimal_set_constant_significant(
                          value, BIGDECIMAL_CONSTANT_PI, 20, (BigDecimalRoundingMode)99));
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT,
                      bigdecimal_set_constant_significant(
                          NULL, BIGDECIMAL_CONSTANT_PI, 20, BIGDECIMAL_ROUND_HALF_EVEN));

    char *text = decimal_text(value);
    TEST_ASSERT_EQUAL_STRING("3.141592653589793238462643383279502884197", text);
    free(text);
    bigdecimal_destroy(value);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_stored_precision_and_rounding);
    RUN_TEST(test_dynamic_values_extend_stored_prefixes);
    RUN_TEST(test_aliasing_and_failure_safety);
    return UNITY_END();
}
