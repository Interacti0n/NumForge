#include <stdint.h>
#include <unity.h>

#include "parser.h"

/* ============================================================
   Reproducible bounded parser fuzz smoke (no evaluation)
   ============================================================ */

void setUp(void) {}
void tearDown(void) {}

static void test_random_bytes_have_repeatable_parse_results(void)
{
    uint32_t state = UINT32_C(0x6e756d66);
    static const unsigned char alphabet[] = "0123eE+−-*/^!().,; \t\nπφ²³_ab\xff";
    for (size_t trial = 0; trial < 3000; trial++)
    {
        char input[65];
        size_t length = trial % sizeof(input);
        for (size_t i = 0; i < length; i++)
        {
            state = state * UINT32_C(1664525) + UINT32_C(1013904223);
            input[i] = (char)alphabet[(state >> 16) % (sizeof(alphabet) - 1U)];
        }
        input[length] = '\0';
        CalculatorExpression *first = NULL;
        CalculatorExpression *second = NULL;
        CalculatorError a, b;
        CalculatorStatus status = calculator_parse(input, &first, &a);
        CalculatorStatus repeated = calculator_parse(input, &second, &b);
        calculator_expression_destroy(first);
        calculator_expression_destroy(second);
        TEST_ASSERT_EQUAL(status, repeated);
        TEST_ASSERT_EQUAL(status, a.status);
        TEST_ASSERT_EQUAL_UINT(a.offset, b.offset);
        TEST_ASSERT_TRUE(a.offset <= length);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_random_bytes_have_repeatable_parse_results);
    return UNITY_END();
}
