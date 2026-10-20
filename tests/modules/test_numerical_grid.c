#include <numforge/numerical.h>
#include <numforge/runtime.h>
#include "src/runtime/numforge_alloc.h"
#include <unity.h>
#include <stdio.h>
#include <stdlib.h>

static BigRational *lower, *upper, *span, *expected, *fraction;
static BigDecimal *number;
static size_t calls, points, fail_at;
static bool expire;
void setUp(void)
{
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_TRUE(numforge_alloc_stats_track(true));
#endif
    lower = bigrational_create(); upper = bigrational_create(); span = bigrational_create();
    expected = bigrational_create(); fraction = bigrational_create(); number = bigdecimal_create();
    TEST_ASSERT_NOT_NULL(lower); TEST_ASSERT_NOT_NULL(upper); TEST_ASSERT_NOT_NULL(span);
    TEST_ASSERT_NOT_NULL(expected); TEST_ASSERT_NOT_NULL(fraction); TEST_ASSERT_NOT_NULL(number);
    calls = fail_at = 0; points = 4; expire = false;
}
void tearDown(void)
{
    numforge_test_allocator_end(); numforge_test_budget_expire_after(0); numforge_budget_end();
    bigrational_destroy(lower); bigrational_destroy(upper); bigrational_destroy(span);
    bigrational_destroy(expected); bigrational_destroy(fraction); bigdecimal_destroy(number);
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_TRUE(numforge_alloc_stats_complete()); TEST_ASSERT_EQUAL_UINT64(0, numforge_alloc_stats_live());
    TEST_ASSERT_TRUE(numforge_alloc_stats_track(false));
#endif
}
static void set(BigRational *value, const char *text)
{
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(number, text));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_from_bigdecimal(value, number));
}
/* Independent oracle: (x-lower)*(count-1) == (upper-lower)*index.
 * No repeated-addition implementation is used to construct expected points. */
static NumForgeNumericalStatus verify(const BigRational *x, size_t index, void *user)
{
    TEST_ASSERT_EQUAL_PTR(&calls, user); TEST_ASSERT_EQUAL_UINT64(calls, index); ++calls;
    int comparison = 1;
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_sub(expected, x, lower));
    char text[32]; (void)snprintf(text, sizeof(text), "%zu", points - 1);
    set(fraction, text);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_mul(expected, expected, fraction));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_sub(span, upper, lower));
    (void)snprintf(text, sizeof(text), "%zu", index); set(fraction, text);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_mul(span, span, fraction));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_compare(&comparison, expected, span));
    TEST_ASSERT_EQUAL_INT(0, comparison);
    if (index == 0 || index + 1 == points) {
        TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_compare(&comparison, x, index ? upper : lower));
        TEST_ASSERT_EQUAL_INT(0, comparison);
    }
    return NUMFORGE_NUMERICAL_OK;
}
static NumForgeNumericalStatus count_visit(const BigRational *x, size_t index, void *user)
{
    (void)x; (void)user;
    TEST_ASSERT_EQUAL_UINT64(calls, index); ++calls;
    if (expire) numforge_test_budget_expire_after(1);
    return fail_at && calls == fail_at ? NUMFORGE_NUMERICAL_CALLBACK_FAILED : NUMFORGE_NUMERICAL_OK;
}
static void test_exact_fractional_grid(void)
{
    set(lower, "0"); set(upper, "1");
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, numforge_grid_visit(lower, upper, points, verify, &calls));
    TEST_ASSERT_EQUAL_UINT64(points, calls);
    calls = 0; points = 2;
    set(lower, "-7.5"); set(upper, "-0.125");
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, numforge_grid_visit(lower, upper, points, verify, &calls));
    TEST_ASSERT_EQUAL_UINT64(points, calls);
}
static void test_thousand_digit_offsets_and_scales(void)
{
    set(lower, "1E1000"); set(span, "1E-1000");
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_add(upper, lower, span));
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, numforge_grid_visit(lower, upper, points, verify, &calls));
    TEST_ASSERT_EQUAL_UINT64(points, calls);
    calls = 0; set(lower, "-1E-1000"); set(upper, "1E-1000"); points = 8;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, numforge_grid_visit(lower, upper, points, verify, &calls));
    TEST_ASSERT_EQUAL_UINT64(points, calls);
}
static void test_validation_and_callback_stop(void)
{
    set(upper, "1");
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_NULL_ARGUMENT, numforge_grid_visit(NULL, upper, 2, count_visit, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_NULL_ARGUMENT, numforge_grid_visit(lower, NULL, 2, count_visit, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_NULL_ARGUMENT, numforge_grid_visit(lower, upper, 2, NULL, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, numforge_grid_visit(lower, upper, 0, count_visit, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, numforge_grid_visit(lower, upper, 1, count_visit, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, numforge_grid_visit(upper, lower, 2, count_visit, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, numforge_grid_visit(upper, upper, 2, count_visit, NULL));
    TEST_ASSERT_EQUAL_UINT64(0, calls);
    fail_at = 2;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_CALLBACK_FAILED, numforge_grid_visit(lower, upper, 4, count_visit, NULL));
    TEST_ASSERT_EQUAL_UINT64(2, calls);
    calls = 0; fail_at = 1;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_CALLBACK_FAILED, numforge_grid_visit(lower, upper, SIZE_MAX, count_visit, NULL));
    TEST_ASSERT_EQUAL_UINT64(1, calls);
}
static void test_budget_before_and_after_callback(void)
{
    set(upper, "1");
    TEST_ASSERT_TRUE(numforge_budget_begin(0, SIZE_MAX, SIZE_MAX));
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OUT_OF_MEMORY, numforge_grid_visit(lower, upper, 4, count_visit, NULL));
    TEST_ASSERT_EQUAL_UINT64(0, calls); TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_TIME, numforge_budget_failure());
    TEST_ASSERT_FALSE(numforge_budget_begin(UINT64_MAX, SIZE_MAX, SIZE_MAX)); numforge_budget_end();
    TEST_ASSERT_TRUE(numforge_budget_begin(UINT64_MAX, SIZE_MAX, SIZE_MAX)); expire = true; fail_at = 1;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OUT_OF_MEMORY, numforge_grid_visit(lower, upper, 4, count_visit, NULL));
    TEST_ASSERT_EQUAL_UINT64(1, calls); TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_TIME, numforge_budget_failure());
    numforge_test_budget_expire_after(0); numforge_budget_end(); calls = 0; expire = false; fail_at = 0;
    TEST_ASSERT_TRUE(numforge_budget_begin(UINT64_MAX, 1, 1));
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OUT_OF_MEMORY, numforge_grid_visit(lower, upper, 4, count_visit, NULL));
    TEST_ASSERT_EQUAL_UINT64(0, calls); TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_MEMORY, numforge_budget_failure());
}
static void test_every_allocation_failure_cleans_up(void)
{
    set(lower, "-1"); set(upper, "1");
    bool complete = false;
    for (size_t failure = 1; failure < 4000; ++failure) {
        calls = 0; numforge_test_allocator_begin(failure);
        NumForgeNumericalStatus status = numforge_grid_visit(lower, upper, 4, count_visit, NULL);
        numforge_test_allocator_end();
        if (status == NUMFORGE_NUMERICAL_OK) { TEST_ASSERT_EQUAL_UINT64(4, calls); complete = true; break; }
        TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OUT_OF_MEMORY, status);
        TEST_ASSERT_TRUE(calls <= 4);
        char *text = NULL;
        TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_to_string(lower, &text));
        TEST_ASSERT_EQUAL_STRING("-1", text); free(text);
        TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_to_string(upper, &text));
        TEST_ASSERT_EQUAL_STRING("1", text); free(text);
    }
    TEST_ASSERT_TRUE(complete);
}
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_exact_fractional_grid);
    RUN_TEST(test_thousand_digit_offsets_and_scales);
    RUN_TEST(test_validation_and_callback_stop);
    RUN_TEST(test_budget_before_and_after_callback);
    RUN_TEST(test_every_allocation_failure_cleans_up);
    return UNITY_END();
}
