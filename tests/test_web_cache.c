#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "web_api.h"

/*
------------------------------------------------------------------------------------------------------------------------------
    Cache correctness: compare every formatted answer with fresh evaluation.
    Hits are asserted explicitly rather than inferred from wall-clock timings.
------------------------------------------------------------------------------------------------------------------------------
*/

static NumForgeWebCache cache;

void setUp(void)
{
    memset(&cache, 0, sizeof(cache));
}

void tearDown(void)
{
    numforge_web_cache_clear(&cache);
}

static void check_value(const char *input, int64_t scale, CalculatorAngleUnit angle,
                        uint64_t revision, bool expected_hit)
{
    CalculatorError error;
    char *cached = NULL;
    char *fresh = NULL;
    bool reused = false;
    CalculatorStatus status = numforge_web_evaluate_cached(
        &cache, revision, input, scale, angle, &cached, &error, &reused);

    TEST_ASSERT_EQUAL(CALCULATOR_OK, status);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, error.status);
    TEST_ASSERT_EQUAL(expected_hit, reused);
    TEST_ASSERT_EQUAL(CALCULATOR_OK,
        numforge_web_evaluate_with_options(input, scale, angle, &fresh, &error));
    TEST_ASSERT_EQUAL_STRING(fresh, cached);
    free(cached);
    free(fresh);
}

static void test_exact_results_reformat_across_precision_changes(void)
{
    check_value("100!", 10, CALCULATOR_ANGLE_RADIANS, 1U, false);
    check_value("100!", 100, CALCULATOR_ANGLE_RADIANS, 2U, true);
    check_value("100!", -1, CALCULATOR_ANGLE_RADIANS, 3U, true);
    check_value("100!", 0, CALCULATOR_ANGLE_RADIANS, 4U, true);
    check_value("pow(1.25;3)+isqrt(99)", 2, CALCULATOR_ANGLE_RADIANS, 5U, false);
    check_value("pow(1.25;3)+isqrt(99)", 100, CALCULATOR_ANGLE_RADIANS, 6U, true);
    check_value("sum(1.25;2.75)+product(2;3)", 2, CALCULATOR_ANGLE_RADIANS, 7U, false);
    check_value("sum(1.25;2.75)+product(2;3)", 100, CALCULATOR_ANGLE_RADIANS, 8U, true);
}

static void test_context_changes_recompute_in_both_directions(void)
{
    check_value("1/3", 10, CALCULATOR_ANGLE_RADIANS, 1U, false);
    check_value("1/3", 20, CALCULATOR_ANGLE_RADIANS, 2U, true);
    check_value("1/3", 50, CALCULATOR_ANGLE_RADIANS, 3U, false);
    check_value("1/3", 10, CALCULATOR_ANGLE_RADIANS, 4U, false);
    check_value("1/3", -1, CALCULATOR_ANGLE_RADIANS, 5U, true);
    check_value("mean(1;2;2)", 10, CALCULATOR_ANGLE_RADIANS, 6U, false);
    check_value("mean(1;2;2)", 50, CALCULATOR_ANGLE_RADIANS, 7U, false);
    check_value("sin(90)", 10, CALCULATOR_ANGLE_DEGREES, 8U, false);
    check_value("sin(90)", 10, CALCULATOR_ANGLE_RADIANS, 9U, false);
}

static void test_integer_looking_result_is_not_an_exactness_proof(void)
{
    check_value("(1E40+1)/3*3-1E40", 10, CALCULATOR_ANGLE_RADIANS, 1U, false);
    check_value("(1E40+1)/3*3-1E40", 50, CALCULATOR_ANGLE_RADIANS, 2U, false);
    check_value("sin(0)", 10, CALCULATOR_ANGLE_RADIANS, 3U, false);
    check_value("sin(0)", 50, CALCULATOR_ANGLE_RADIANS, 4U, false);
    check_value("sqrt(4)", 10, CALCULATOR_ANGLE_RADIANS, 5U, false);
    check_value("sqrt(4)", 50, CALCULATOR_ANGLE_RADIANS, 6U, false);
}

static void test_failures_and_older_requests_do_not_replace_success(void)
{
    CalculatorError error;
    char *text = NULL;
    bool reused = true;

    check_value("2+2", 10, CALCULATOR_ANGLE_RADIANS, 10U, false);
    TEST_ASSERT_EQUAL(CALCULATOR_DIVISION_BY_ZERO,
        numforge_web_evaluate_cached(&cache, 12U, "1/0", 10,
            CALCULATOR_ANGLE_RADIANS, &text, &error, &reused));
    TEST_ASSERT_NULL(text);
    TEST_ASSERT_FALSE(reused);
    check_value("7", 10, CALCULATOR_ANGLE_RADIANS, 11U, false);
    check_value("2+2", 10, CALCULATOR_ANGLE_RADIANS, 13U, true);
    check_value("8", 10, CALCULATOR_ANGLE_RADIANS, 13U, false);
    check_value("2+2", 10, CALCULATOR_ANGLE_RADIANS, 14U, true);
    numforge_web_cache_clear(&cache);
    check_value("2+2", 10, CALCULATOR_ANGLE_RADIANS, 15U, false);
}

static void test_clients_are_independent(void)
{
    NumForgeWebCache other = {0};
    CalculatorError error;
    char *text = NULL;
    bool reused = true;

    check_value("42", 10, CALCULATOR_ANGLE_RADIANS, 1U, false);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, numforge_web_evaluate_cached(
        &other, 1U, "42", 10, CALCULATOR_ANGLE_RADIANS, &text, &error, &reused));
    TEST_ASSERT_FALSE(reused);
    free(text);
    numforge_web_cache_clear(&other);
    check_value("42", 10, CALCULATOR_ANGLE_RADIANS, 2U, true);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_exact_results_reformat_across_precision_changes);
    RUN_TEST(test_context_changes_recompute_in_both_directions);
    RUN_TEST(test_integer_looking_result_is_not_an_exactness_proof);
    RUN_TEST(test_failures_and_older_requests_do_not_replace_success);
    RUN_TEST(test_clients_are_independent);
    return UNITY_END();
}
