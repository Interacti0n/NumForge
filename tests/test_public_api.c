#include <unity.h>
#include <numforge/bigdecimal.h>
#include <numforge/runtime.h>
#include <stdlib.h>
#include "package_consumer/check_api.h"

void setUp(void) { numforge_budget_end(); }
void tearDown(void) { numforge_budget_end(); }

static void test_public_pipeline(void) { TEST_ASSERT_EQUAL_INT(0, public_api_checks()); }

static void test_domain_and_failure_contracts(void)
{
    BigInt *n = bigint_create();
    BigDecimal *a = bigdecimal_create();
    char marker = 'x', *text = &marker;
    TEST_ASSERT_NOT_NULL(n); TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(n, "7"));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(a, "1.25"));
    TEST_ASSERT_EQUAL(BIGDECIMAL_INVALID_ARGUMENT, bigdecimal_to_bigint(n, a));
    char *actual = bigint_to_string(n);
    TEST_ASSERT_EQUAL_STRING("7", actual); free(actual);
    TEST_ASSERT_EQUAL(BIGDECIMAL_NULL_ARGUMENT, bigdecimal_format(NULL, -1, BIGDECIMAL_ROUND_HALF_EVEN, &text));
    TEST_ASSERT_EQUAL_PTR(&marker, text);
    TEST_ASSERT_EQUAL(BIGDECIMAL_INVALID_ARGUMENT, bigdecimal_format(a, -2, BIGDECIMAL_ROUND_HALF_EVEN, &text));
    TEST_ASSERT_EQUAL_PTR(&marker, text);
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(n, "-1"));
    TEST_ASSERT_EQUAL(BIGDECIMAL_INVALID_ARGUMENT, bigdecimal_pow(a, a, n));
    TEST_ASSERT_EQUAL(BIGINT_NEGATIVE_ARGUMENT, bigint_isqrt(n, n));
    TEST_ASSERT_EQUAL(BIGDECIMAL_INVALID_ARGUMENT, bigdecimal_set_constant(a, (BigDecimalConstant)99));
    text = NULL;
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_to_string(a, &text));
    TEST_ASSERT_EQUAL_STRING("1.25", text); free(text);
    TEST_ASSERT_TRUE(numforge_budget_begin(0, SIZE_MAX, SIZE_MAX));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OUT_OF_MEMORY, bigdecimal_sqrt(a, a, 34, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_TIME, numforge_budget_failure());
    numforge_budget_end();
    text = NULL;
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_to_string(a, &text));
    TEST_ASSERT_EQUAL_STRING("1.25", text); free(text);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(a, "1"));
    /* No calculator degree/precision caps are embedded in numeric APIs. */
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_root(a, a, 10001, 10005, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_TRUE(numforge_budget_begin(5000, 1, 1));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OUT_OF_MEMORY, bigdecimal_sqrt(a, a, 34, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_MEMORY, numforge_budget_failure());
    numforge_budget_end();
    bigint_destroy(n); bigdecimal_destroy(a);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_public_pipeline);
    RUN_TEST(test_domain_and_failure_contracts);
    return UNITY_END();
}
