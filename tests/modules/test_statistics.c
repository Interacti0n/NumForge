#include <numforge/statistics.h>
#include <numforge/runtime.h>
#include "src/runtime/numforge_alloc.h"
#include <unity.h>
#include <stdlib.h>
#include <string.h>

static BigRational *samples[8];
static NumForgeStatisticsSummary *summary;
void setUp(void)
{
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_TRUE(numforge_alloc_stats_track(true));
#endif
}
void tearDown(void)
{
    numforge_test_allocator_end(); numforge_test_budget_expire_after(0); numforge_budget_end();
    for (size_t i = 0; i < 8; ++i) { bigrational_destroy(samples[i]); samples[i] = NULL; }
    numforge_statistics_summary_destroy(summary); summary = NULL;
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_TRUE(numforge_alloc_stats_complete());
    TEST_ASSERT_EQUAL_UINT64(0, numforge_alloc_stats_live());
    TEST_ASSERT_TRUE(numforge_alloc_stats_track(false));
#endif
}
static BigRational *rational(const char *text)
{
    char buffer[4096]; const char *slash = strchr(text, '/');
    size_t length = slash ? (size_t)(slash - text) : strlen(text);
    BigRational *value = bigrational_create(); BigInt *top = bigint_create(), *bottom = bigint_create();
    TEST_ASSERT_NOT_NULL(value); TEST_ASSERT_NOT_NULL(top); TEST_ASSERT_NOT_NULL(bottom); TEST_ASSERT_TRUE(length < sizeof(buffer));
    memcpy(buffer, text, length); buffer[length] = '\0';
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(top, buffer));
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(bottom, slash ? slash + 1 : "1"));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_set_fraction(value, top, bottom));
    bigint_destroy(top); bigint_destroy(bottom); return value;
}
static void describe(const char *const *texts, size_t count)
{
    const BigRational *values[8]; TEST_ASSERT_TRUE(count <= 8);
    for (size_t i = 0; i < count; ++i) values[i] = samples[i] = rational(texts[i]);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, numforge_statistics_describe(&summary, values, count));
    TEST_ASSERT_EQUAL_UINT(count, numforge_statistics_summary_count(summary));
}
static void equals(const BigRational *value, const char *expected)
{
    BigRational *reference = rational(expected); int comparison = 99;
    TEST_ASSERT_NOT_NULL(value); TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_compare(&comparison, value, reference));
    TEST_ASSERT_EQUAL_INT(0, comparison); bigrational_destroy(reference);
}
static void test_population_and_sample_reference(void)
{
    const char *values[] = {"2", "4", "4", "4", "5", "5", "7", "9"}; describe(values, 8);
    equals(numforge_statistics_summary_sum(summary), "40"); equals(numforge_statistics_summary_minimum(summary), "2");
    equals(numforge_statistics_summary_maximum(summary), "9"); equals(numforge_statistics_summary_mean(summary), "5");
    equals(numforge_statistics_summary_variance_population(summary), "4"); equals(numforge_statistics_summary_variance_sample(summary), "32/7");
    equals(samples[0], "2"); equals(samples[7], "9");
}
static void test_fractional_signed_samples(void)
{
    const char *values[] = {"-1/3", "1/6", "2/3"}; describe(values, 3);
    equals(numforge_statistics_summary_sum(summary), "1/2"); equals(numforge_statistics_summary_minimum(summary), "-1/3");
    equals(numforge_statistics_summary_maximum(summary), "2/3"); equals(numforge_statistics_summary_mean(summary), "1/6");
    equals(numforge_statistics_summary_variance_population(summary), "1/6"); equals(numforge_statistics_summary_variance_sample(summary), "1/4");
}
static void test_singleton_undefined_sample_and_owned_fields(void)
{
    const char *values[] = {"-9/5"}; describe(values, 1);
    bigrational_destroy(samples[0]); samples[0] = NULL;
    equals(numforge_statistics_summary_sum(summary), "-9/5"); equals(numforge_statistics_summary_minimum(summary), "-9/5");
    equals(numforge_statistics_summary_maximum(summary), "-9/5"); equals(numforge_statistics_summary_mean(summary), "-9/5");
    equals(numforge_statistics_summary_variance_population(summary), "0"); TEST_ASSERT_NULL(numforge_statistics_summary_variance_sample(summary));
}
static void test_repeated_handles_and_borrowed_old_output_inputs(void)
{
    samples[0] = rational("7/11"); const BigRational *values[] = {samples[0], samples[0], samples[0]};
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, numforge_statistics_describe(&summary, values, 3));
    equals(numforge_statistics_summary_sum(summary), "21/11"); equals(numforge_statistics_summary_mean(summary), "7/11");
    equals(numforge_statistics_summary_variance_population(summary), "0"); equals(numforge_statistics_summary_variance_sample(summary), "0");
    NumForgeStatisticsSummary *previous = summary;
    const BigRational *borrowed[] = {numforge_statistics_summary_mean(previous), numforge_statistics_summary_mean(previous)};
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, numforge_statistics_describe(&summary, borrowed, 2));
    numforge_statistics_summary_destroy(previous);
    equals(numforge_statistics_summary_sum(summary), "14/11"); equals(numforge_statistics_summary_mean(summary), "7/11");
}
static void test_nonterminating_mean_and_variances_remain_exact(void)
{
    const char *values[] = {"0", "0", "1"}; describe(values, 3);
    equals(numforge_statistics_summary_mean(summary), "1/3");
    equals(numforge_statistics_summary_variance_population(summary), "2/9"); equals(numforge_statistics_summary_variance_sample(summary), "1/3");
}
static void test_thousand_digit_common_offset(void)
{
    char n[1002], next[1002], last[1002], sum[1002];
    n[0] = '1'; memset(n + 1, '0', 1000); n[1001] = '\0';
    memcpy(next, n, sizeof(n)); next[1000] = '1'; memcpy(last, n, sizeof(n)); last[1000] = '2';
    memcpy(sum, n, sizeof(n)); sum[0] = '3'; sum[1000] = '3';
    const char *values[] = {n, next, last}; describe(values, 3);
    equals(numforge_statistics_summary_sum(summary), sum); equals(numforge_statistics_summary_minimum(summary), n);
    equals(numforge_statistics_summary_maximum(summary), last); equals(numforge_statistics_summary_mean(summary), next);
    equals(numforge_statistics_summary_variance_population(summary), "2/3"); equals(numforge_statistics_summary_variance_sample(summary), "1");
}
static void test_thousand_digit_tiny_spread(void)
{
    char first[1004], second[1004], third[1004], population[2004], sample[2004];
    memcpy(first, "1/1", 3); memset(first + 3, '0', 1000); first[1003] = '\0';
    memcpy(second, first, sizeof(first)); second[0] = '2'; memcpy(third, first, sizeof(first)); third[0] = '3';
    memcpy(population, "2/3", 3); memset(population + 3, '0', 2000); population[2003] = '\0';
    memcpy(sample, population, sizeof(sample)); sample[0] = '1'; sample[2] = '1';
    const char *values[] = {first, second, third}; describe(values, 3);
    equals(numforge_statistics_summary_mean(summary), second); equals(numforge_statistics_summary_minimum(summary), first);
    equals(numforge_statistics_summary_maximum(summary), third);
    equals(numforge_statistics_summary_variance_population(summary), population); equals(numforge_statistics_summary_variance_sample(summary), sample);
}
static void test_validation_null_queries_and_failure_preservation(void)
{
    const char *texts[] = {"0", "1"}; describe(texts, 2);
    NumForgeStatisticsSummary *saved = summary; const BigRational *values[] = {samples[0], NULL};
    TEST_ASSERT_EQUAL(BIGRATIONAL_NULL_ARGUMENT, numforge_statistics_describe(NULL, values, 1));
    TEST_ASSERT_EQUAL(BIGRATIONAL_NULL_ARGUMENT, numforge_statistics_describe(&summary, NULL, 1));
    TEST_ASSERT_EQUAL(BIGRATIONAL_NULL_ARGUMENT, numforge_statistics_describe(&summary, values, 2));
    TEST_ASSERT_EQUAL(BIGRATIONAL_INVALID_ARGUMENT, numforge_statistics_describe(&summary, values, 0));
    TEST_ASSERT_EQUAL(BIGRATIONAL_VALUE_TOO_LARGE, numforge_statistics_describe(&summary, values, SIZE_MAX));
    TEST_ASSERT_EQUAL_PTR(saved, summary); equals(numforge_statistics_summary_mean(summary), "1/2");
    TEST_ASSERT_EQUAL_UINT(0, numforge_statistics_summary_count(NULL));
    TEST_ASSERT_NULL(numforge_statistics_summary_sum(NULL)); TEST_ASSERT_NULL(numforge_statistics_summary_minimum(NULL));
    TEST_ASSERT_NULL(numforge_statistics_summary_maximum(NULL)); TEST_ASSERT_NULL(numforge_statistics_summary_mean(NULL));
    TEST_ASSERT_NULL(numforge_statistics_summary_variance_population(NULL)); TEST_ASSERT_NULL(numforge_statistics_summary_variance_sample(NULL));
    numforge_statistics_summary_destroy(NULL);
}
static void test_allocation_and_budget_failures_are_atomic(void)
{
    const char *texts[] = {"-1/3", "1/6", "2/3"}; describe(texts, 3);
    NumForgeStatisticsSummary *saved = summary; const BigRational *values[] = {samples[0], samples[1], samples[2]};
    for (size_t count = 1; count <= 3; count += 2) {
        bool complete = false;
        for (size_t failure = 1; failure < 8192; ++failure) {
            numforge_test_allocator_begin(failure);
            BigRationalStatus status = numforge_statistics_describe(&summary, values, count);
            bool injected = numforge_test_allocator_did_fail(); numforge_test_allocator_end();
            if (status == BIGRATIONAL_OK) { numforge_statistics_summary_destroy(summary); summary = saved; }
            else { TEST_ASSERT_EQUAL(BIGRATIONAL_OUT_OF_MEMORY, status); TEST_ASSERT_EQUAL_PTR(saved, summary); }
            equals(numforge_statistics_summary_mean(saved), "1/6"); equals(samples[0], "-1/3");
            if (!injected) { complete = true; break; }
        }
        TEST_ASSERT_TRUE(complete);
    }
    const size_t checks[] = {1, 64, 512};
    for (unsigned mode = 0; mode < 5; ++mode) {
        TEST_ASSERT_TRUE(numforge_budget_begin(5000, mode == 0 ? 0 : SIZE_MAX, mode == 1 ? 0 : SIZE_MAX));
        if (mode >= 2) numforge_test_budget_expire_after(checks[mode - 2]);
        TEST_ASSERT_EQUAL(BIGRATIONAL_OUT_OF_MEMORY, numforge_statistics_describe(&summary, values, 3));
        TEST_ASSERT_EQUAL_PTR(saved, summary);
        TEST_ASSERT_EQUAL(mode < 2 ? NUMFORGE_BUDGET_MEMORY : NUMFORGE_BUDGET_TIME, numforge_budget_failure());
        TEST_ASSERT_FALSE(numforge_budget_begin(5000, SIZE_MAX, SIZE_MAX)); numforge_budget_end();
    }
    TEST_ASSERT_TRUE(numforge_budget_begin(5000, SIZE_MAX, SIZE_MAX));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, numforge_statistics_describe(&summary, values, 3));
    TEST_ASSERT_FALSE(numforge_budget_begin(5000, SIZE_MAX, SIZE_MAX)); numforge_budget_end();
    numforge_statistics_summary_destroy(saved);
}
static void test_covariance_pairing_sign_singleton_and_aliases(void)
{
    const char *texts[] = {"1", "2", "3", "3", "1", "-1", "99"};
    for (size_t i = 0; i < 7; ++i) samples[i] = rational(texts[i]);
    const BigRational *x[] = {samples[0], samples[1], samples[2]};
    const BigRational *y[] = {samples[3], samples[4], samples[5]};
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, numforge_statistics_covariance(samples[6], x, y, 3, false)); equals(samples[6], "-4/3");
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, numforge_statistics_covariance(samples[6], y, x, 3, true)); equals(samples[6], "-2");
    const BigRational *repeated[] = {samples[0], samples[0], samples[0]};
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, numforge_statistics_covariance(samples[6], repeated, y, 3, true)); equals(samples[6], "0");
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, numforge_statistics_covariance(samples[6], x, y, 1, false)); equals(samples[6], "0");
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, numforge_statistics_covariance(samples[0], x, y, 3, true)); equals(samples[0], "-2");
    equals(samples[1], "2"); equals(samples[3], "3");
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, numforge_statistics_covariance(samples[4], y, y, 3, true)); equals(samples[4], "4");
}
static void test_covariance_fractional_and_thousand_digit_scales(void)
{
    samples[0] = rational("-1/3"); samples[1] = rational("1/6"); samples[2] = rational("2/3");
    samples[3] = rational("0");
    const BigRational *x[] = {samples[0], samples[1], samples[2]};
    const BigRational *y[] = {samples[2], samples[0], samples[1]};
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, numforge_statistics_covariance(samples[3], x, y, 3, false)); equals(samples[3], "-1/12");
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, numforge_statistics_covariance(samples[3], x, y, 3, true)); equals(samples[3], "-1/8");
    char text[1004]; text[0] = '1'; memset(text + 1, '0', 1000); text[1001] = '\0';
    for (size_t i = 0; i < 3; ++i) {
        text[1000] = (char)('0' + i); bigrational_destroy(samples[i]); samples[i] = rational(text);
    }
    const BigRational *large[] = {samples[0], samples[1], samples[2]};
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, numforge_statistics_covariance(samples[3], large, large, 3, true)); equals(samples[3], "1");
    memcpy(text, "1/1", 3); memset(text + 3, '0', 1000); text[1003] = '\0';
    for (size_t i = 0; i < 3; ++i) { text[0] = (char)('1' + i); samples[4 + i] = rational(text); }
    const BigRational *tiny[] = {samples[6], samples[5], samples[4]};
    /* Mixed enormous offsets and tiny reversed spreads: sample covariance -10^-1000. */
    char expected[1005]; memcpy(expected, "-1/1", 4); memset(expected + 4, '0', 1000); expected[1004] = '\0';
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, numforge_statistics_covariance(samples[3], large, tiny, 3, true)); equals(samples[3], expected);
}
static void test_covariance_validation_and_atomic_failures(void)
{
    samples[0] = rational("-1/3"); samples[1] = rational("1/6"); samples[2] = rational("2/3");
    const BigRational *x[] = {samples[0], samples[1], samples[2]}, *bad[] = {samples[0], NULL};
    TEST_ASSERT_EQUAL(BIGRATIONAL_NULL_ARGUMENT, numforge_statistics_covariance(NULL, x, x, 3, false));
    TEST_ASSERT_EQUAL(BIGRATIONAL_NULL_ARGUMENT, numforge_statistics_covariance(samples[0], NULL, x, 3, false));
    TEST_ASSERT_EQUAL(BIGRATIONAL_NULL_ARGUMENT, numforge_statistics_covariance(samples[0], x, NULL, 3, false));
    TEST_ASSERT_EQUAL(BIGRATIONAL_NULL_ARGUMENT, numforge_statistics_covariance(samples[0], x, bad, 2, false));
    TEST_ASSERT_EQUAL(BIGRATIONAL_NULL_ARGUMENT, numforge_statistics_covariance(samples[0], bad, x, 2, false));
    TEST_ASSERT_EQUAL(BIGRATIONAL_INVALID_ARGUMENT, numforge_statistics_covariance(samples[0], x, x, 0, false));
    TEST_ASSERT_EQUAL(BIGRATIONAL_INVALID_ARGUMENT, numforge_statistics_covariance(samples[0], x, x, 1, true));
    TEST_ASSERT_EQUAL(BIGRATIONAL_VALUE_TOO_LARGE, numforge_statistics_covariance(samples[0], x, x, SIZE_MAX, false));
    for (unsigned mode = 0; mode < 3; ++mode) {
        bool complete = false;
        for (size_t failure = 1; failure < 8192; ++failure) {
            numforge_test_allocator_begin(failure);
            BigRationalStatus status = numforge_statistics_covariance(samples[0], x, x, mode == 0 ? 1 : 3, mode == 2);
            bool injected = numforge_test_allocator_did_fail(); numforge_test_allocator_end();
            if (status == BIGRATIONAL_OK) {
                equals(samples[0], mode == 0 ? "0" : mode == 1 ? "1/6" : "1/4");
                bigrational_destroy(samples[0]); samples[0] = rational("-1/3"); x[0] = samples[0];
            } else { TEST_ASSERT_EQUAL(BIGRATIONAL_OUT_OF_MEMORY, status); equals(samples[0], "-1/3"); }
            equals(samples[1], "1/6"); equals(samples[2], "2/3");
            if (!injected) { complete = true; break; }
        }
        TEST_ASSERT_TRUE(complete);
    }
    const size_t checks[] = {1, 64, 512};
    for (unsigned mode = 0; mode < 5; ++mode) {
        TEST_ASSERT_TRUE(numforge_budget_begin(5000, mode == 0 ? 0 : SIZE_MAX, mode == 1 ? 0 : SIZE_MAX));
        if (mode >= 2) numforge_test_budget_expire_after(checks[mode - 2]);
        TEST_ASSERT_EQUAL(BIGRATIONAL_OUT_OF_MEMORY, numforge_statistics_covariance(samples[0], x, x, 3, true));
        TEST_ASSERT_EQUAL(mode < 2 ? NUMFORGE_BUDGET_MEMORY : NUMFORGE_BUDGET_TIME, numforge_budget_failure());
        TEST_ASSERT_FALSE(numforge_budget_begin(5000, SIZE_MAX, SIZE_MAX)); numforge_budget_end();
        equals(samples[0], "-1/3");
    }
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, numforge_statistics_covariance(samples[0], x, x, 3, true)); equals(samples[0], "1/4");
}
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_population_and_sample_reference);
    RUN_TEST(test_fractional_signed_samples);
    RUN_TEST(test_singleton_undefined_sample_and_owned_fields);
    RUN_TEST(test_repeated_handles_and_borrowed_old_output_inputs);
    RUN_TEST(test_nonterminating_mean_and_variances_remain_exact);
    RUN_TEST(test_thousand_digit_common_offset);
    RUN_TEST(test_thousand_digit_tiny_spread);
    RUN_TEST(test_validation_null_queries_and_failure_preservation);
    RUN_TEST(test_allocation_and_budget_failures_are_atomic);
    RUN_TEST(test_covariance_pairing_sign_singleton_and_aliases);
    RUN_TEST(test_covariance_fractional_and_thousand_digit_scales);
    RUN_TEST(test_covariance_validation_and_atomic_failures);
    return UNITY_END();
}
