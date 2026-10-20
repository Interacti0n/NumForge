#include <numforge/statistics.h>
#include <numforge/runtime.h>
#include "src/runtime/numforge_alloc.h"
#include "tests/support/numbers.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static BigRational *values[6];
static NumForgeLinearRegression *fit;
void setUp(void)
{
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_TRUE(numforge_alloc_stats_track(true));
#endif
}
void tearDown(void)
{
    numforge_test_allocator_end(); numforge_test_budget_expire_after(0); numforge_budget_end();
    for (size_t i = 0; i < 6; ++i) { bigrational_destroy(values[i]); values[i] = NULL; }
    numforge_linear_regression_destroy(fit); fit = NULL;
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_TRUE(numforge_alloc_stats_complete());
    TEST_ASSERT_EQUAL_UINT64(0, numforge_alloc_stats_live());
    TEST_ASSERT_TRUE(numforge_alloc_stats_track(false));
#endif
}
static void dataset(void)
{
    const char *texts[] = {"1", "2", "3", "1", "2", "2"};
    for (size_t i = 0; i < 6; ++i) values[i] = test_fraction(texts[i], "1");
}
static void coefficients(const char *slope, const char *intercept)
{
    test_assert_fraction(slope, numforge_linear_regression_slope(fit));
    test_assert_fraction(intercept, numforge_linear_regression_intercept(fit));
}
static void test_noisy_fit_exact_coefficients_and_independent_lifetime(void)
{
    dataset(); const BigRational *x[] = {values[0], values[1], values[2]}, *y[] = {values[3], values[4], values[5]};
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, numforge_statistics_linear_regression(&fit, x, y, 3));
    TEST_ASSERT_EQUAL_UINT64(3, numforge_linear_regression_count(fit)); coefficients("1/2", "2/3");
    for (size_t i = 0; i < 6; ++i) { bigrational_destroy(values[i]); values[i] = NULL; }
    coefficients("1/2", "2/3");
}
static void test_horizontal_duplicates_two_pairs_and_borrowed_previous_output(void)
{
    dataset(); const BigRational *x[] = {values[0], values[0], values[1]}, *y[] = {values[2], values[2], values[2]};
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, numforge_statistics_linear_regression(&fit, x, y, 3)); coefficients("0", "3");
    NumForgeLinearRegression *previous = fit;
    const BigRational *borrowed[] = {numforge_linear_regression_slope(previous), numforge_linear_regression_intercept(previous)};
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, numforge_statistics_linear_regression(&fit, borrowed, borrowed, 2));
    numforge_linear_regression_destroy(previous);
    TEST_ASSERT_EQUAL_UINT64(2, numforge_linear_regression_count(fit)); coefficients("1", "0");
    test_assert_fraction("1", values[0]); test_assert_fraction("3", values[2]);
}
static void test_fractional_two_point_negative_line(void)
{
    values[0] = test_fraction("1", "3"); values[1] = test_fraction("2", "3");
    values[2] = test_fraction("3", "7"); values[3] = test_fraction("1", "7");
    const BigRational *x[] = {values[0], values[1]}, *y[] = {values[2], values[3]};
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, numforge_statistics_linear_regression(&fit, x, y, 2)); coefficients("-6/7", "5/7");
}
static void test_thousand_digit_offsets_and_spreads_in_both_axes(void)
{
    char power[1002], number[1002], expected[2010];
    power[0] = '1'; memset(power + 1, '0', 1000); power[1001] = '\0'; memcpy(number, power, sizeof(power));
    for (size_t i = 0; i < 3; ++i) {
        number[1000] = (char)('0' + i); values[i] = test_fraction(number, "1");
        const char *top[] = {"3", "2", "1"}; values[i + 3] = test_fraction(top[i], power);
    }
    const BigRational *x[] = {values[0], values[1], values[2]}, *y[] = {values[3], values[4], values[5]};
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, numforge_statistics_linear_regression(&fit, x, y, 3));
    (void)snprintf(expected, sizeof(expected), "-1/%s", power); test_assert_fraction(expected, numforge_linear_regression_slope(fit));
    number[1000] = '3'; (void)snprintf(expected, sizeof(expected), "%s/%s", number, power);
    test_assert_fraction(expected, numforge_linear_regression_intercept(fit));
    numforge_linear_regression_destroy(fit); fit = NULL;
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, numforge_statistics_linear_regression(&fit, y, x, 3));
    (void)snprintf(expected, sizeof(expected), "-%s", power); coefficients(expected, number);
}
static void test_validation_constant_x_and_null_queries_preserve_output(void)
{
    dataset(); const BigRational *x[] = {values[0], values[1]}, *bad[] = {values[0], NULL}, *constant[] = {values[0], values[0]};
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, numforge_statistics_linear_regression(&fit, x, x, 2));
    NumForgeLinearRegression *saved = fit;
    TEST_ASSERT_EQUAL(BIGRATIONAL_NULL_ARGUMENT, numforge_statistics_linear_regression(NULL, x, x, 2));
    TEST_ASSERT_EQUAL(BIGRATIONAL_NULL_ARGUMENT, numforge_statistics_linear_regression(&fit, NULL, x, 2));
    TEST_ASSERT_EQUAL(BIGRATIONAL_NULL_ARGUMENT, numforge_statistics_linear_regression(&fit, x, NULL, 2));
    TEST_ASSERT_EQUAL(BIGRATIONAL_NULL_ARGUMENT, numforge_statistics_linear_regression(&fit, bad, x, 2));
    TEST_ASSERT_EQUAL(BIGRATIONAL_NULL_ARGUMENT, numforge_statistics_linear_regression(&fit, x, bad, 2));
    TEST_ASSERT_EQUAL(BIGRATIONAL_INVALID_ARGUMENT, numforge_statistics_linear_regression(&fit, x, x, 0));
    TEST_ASSERT_EQUAL(BIGRATIONAL_INVALID_ARGUMENT, numforge_statistics_linear_regression(&fit, x, x, 1));
    TEST_ASSERT_EQUAL(BIGRATIONAL_INVALID_ARGUMENT, numforge_statistics_linear_regression(&fit, constant, x, 2));
    TEST_ASSERT_EQUAL(BIGRATIONAL_INVALID_ARGUMENT, numforge_statistics_linear_regression(&fit, constant, constant, 2));
    TEST_ASSERT_EQUAL(BIGRATIONAL_VALUE_TOO_LARGE, numforge_statistics_linear_regression(&fit, x, x, SIZE_MAX));
    TEST_ASSERT_EQUAL_PTR(saved, fit); coefficients("1", "0"); test_assert_fraction("1", values[0]);
    TEST_ASSERT_EQUAL_UINT64(0, numforge_linear_regression_count(NULL));
    TEST_ASSERT_NULL(numforge_linear_regression_slope(NULL)); TEST_ASSERT_NULL(numforge_linear_regression_intercept(NULL));
    numforge_linear_regression_destroy(NULL);
}
static void test_each_allocation_failure_is_atomic_for_noisy_and_horizontal_fits(void)
{
    dataset(); const BigRational *x[] = {values[0], values[1], values[2]}, *y[] = {values[3], values[4], values[5]};
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, numforge_statistics_linear_regression(&fit, x, y, 3));
    NumForgeLinearRegression *saved = fit;
    for (unsigned mode = 0; mode < 2; ++mode) {
        if (mode) y[1] = y[2] = y[0];
        bool complete = false;
        for (size_t failure = 1; failure < 8192; ++failure) {
            numforge_test_allocator_begin(failure);
            BigRationalStatus status = numforge_statistics_linear_regression(&fit, x, y, 3);
            bool injected = numforge_test_allocator_did_fail(); numforge_test_allocator_end();
            if (status == BIGRATIONAL_OK) { numforge_linear_regression_destroy(fit); fit = saved; }
            else { TEST_ASSERT_EQUAL(BIGRATIONAL_OUT_OF_MEMORY, status); TEST_ASSERT_EQUAL_PTR(saved, fit); }
            coefficients("1/2", "2/3"); test_assert_fraction("1", values[0]); test_assert_fraction("2", values[5]);
            if (!injected) { complete = true; break; }
        }
        TEST_ASSERT_TRUE(complete);
    }
}
static void test_inherited_budget_failure_and_recovery(void)
{
    dataset(); const BigRational *x[] = {values[0], values[1], values[2]}, *y[] = {values[3], values[4], values[5]};
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, numforge_statistics_linear_regression(&fit, x, y, 3));
    NumForgeLinearRegression *saved = fit; const size_t checks[] = {1, 32, 256};
    for (unsigned mode = 0; mode < 5; ++mode) {
        TEST_ASSERT_TRUE(numforge_budget_begin(5000, mode == 0 ? 0 : SIZE_MAX, mode == 1 ? 0 : SIZE_MAX));
        if (mode >= 2) numforge_test_budget_expire_after(checks[mode - 2]);
        TEST_ASSERT_EQUAL(BIGRATIONAL_OUT_OF_MEMORY, numforge_statistics_linear_regression(&fit, x, y, 3));
        TEST_ASSERT_EQUAL_PTR(saved, fit);
        TEST_ASSERT_EQUAL(mode < 2 ? NUMFORGE_BUDGET_MEMORY : NUMFORGE_BUDGET_TIME, numforge_budget_failure());
        TEST_ASSERT_FALSE(numforge_budget_begin(5000, SIZE_MAX, SIZE_MAX));
        numforge_test_budget_expire_after(0); numforge_budget_end(); coefficients("1/2", "2/3");
    }
    TEST_ASSERT_TRUE(numforge_budget_begin(5000, SIZE_MAX, SIZE_MAX));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, numforge_statistics_linear_regression(&fit, x, y, 3));
    TEST_ASSERT_FALSE(numforge_budget_begin(5000, SIZE_MAX, SIZE_MAX)); numforge_budget_end();
    numforge_linear_regression_destroy(saved); coefficients("1/2", "2/3");
}
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_noisy_fit_exact_coefficients_and_independent_lifetime);
    RUN_TEST(test_horizontal_duplicates_two_pairs_and_borrowed_previous_output);
    RUN_TEST(test_fractional_two_point_negative_line);
    RUN_TEST(test_thousand_digit_offsets_and_spreads_in_both_axes);
    RUN_TEST(test_validation_constant_x_and_null_queries_preserve_output);
    RUN_TEST(test_each_allocation_failure_is_atomic_for_noisy_and_horizontal_fits);
    RUN_TEST(test_inherited_budget_failure_and_recovery);
    return UNITY_END();
}
