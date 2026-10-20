#include "examples/sdk/phase_function.h"
#include "src/runtime/numforge_alloc.h"
#include <unity.h>
#include <stdlib.h>

static PhaseFunction f;
static NumForgePhaseOptions options;
static NumForgePhaseLocator *locator;
static NumForgePhaseReport report;
static bool unusable, expire;
static NumForgeNumericalStatus failure;
void setUp(void)
{
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_TRUE(numforge_alloc_stats_track(true));
#endif
    TEST_ASSERT_TRUE(phase_function_init(&f));
    numforge_phase_options_init(&options); options.period = f.period; options.tolerance = f.tolerance;
    report = (NumForgePhaseReport){(NumForgePhaseKind)99, 77}; unusable = expire = false; failure = NUMFORGE_NUMERICAL_OK;
}
void tearDown(void)
{
    numforge_test_allocator_end(); numforge_test_budget_expire_after(0); numforge_budget_end();
    numforge_phase_locator_destroy(locator); locator = NULL; phase_function_destroy(&f);
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_TRUE(numforge_alloc_stats_complete()); TEST_ASSERT_EQUAL_UINT64(0, numforge_alloc_stats_live());
    TEST_ASSERT_TRUE(numforge_alloc_stats_track(false));
#endif
}
static void set(BigDecimal *value, const char *text)
{ TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, text)); }
static void create(void)
{
    numforge_phase_locator_destroy(locator); locator = NULL;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, numforge_phase_locator_create(&locator, &options));
}
static NumForgeNumericalStatus evaluate(BigDecimal *value, bool *usable, const BigRational *x, void *user)
{
    TEST_ASSERT_FALSE(*usable);
    if (expire) numforge_test_budget_expire_after(1);
    if (failure != NUMFORGE_NUMERICAL_OK) return failure;
    if (unusable) return NUMFORGE_NUMERICAL_OK;
    return phase_function_evaluate(value, usable, x, user);
}
static NumForgeNumericalStatus run(void)
{ return numforge_phase_locate(&report, locator, f.lower, f.upper, f.phase_lower, f.phase_upper, f.offset, evaluate, &f); }
static void expect(NumForgePhaseKind kind, size_t evaluations)
{
    f.calls = 0; TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, run()); TEST_ASSERT_EQUAL(kind, report.kind);
    TEST_ASSERT_EQUAL_UINT64(evaluations, report.evaluations);
    if (kind == NUMFORGE_PHASE_FOUND) {
        int comparison = 1;
        TEST_ASSERT_NOT_NULL(numforge_phase_locator_target(locator));
        TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_compare(&comparison, f.last, numforge_phase_locator_x(locator)));
        TEST_ASSERT_EQUAL_INT(0, comparison);
    } else { TEST_ASSERT_NULL(numforge_phase_locator_x(locator)); TEST_ASSERT_NULL(numforge_phase_locator_target(locator)); }
}
static void preserved(void)
{ TEST_ASSERT_EQUAL_INT(99, report.kind); TEST_ASSERT_EQUAL_UINT64(77, report.evaluations); }
static void test_linear_phase_offsets_directions_and_owned_policy(void)
{
    create(); set(f.period, "-1"); set(f.tolerance, "-1"); expect(NUMFORGE_PHASE_FOUND, 1);
    char *text = NULL; TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_to_string(numforge_phase_locator_x(locator), &text));
    TEST_ASSERT_EQUAL_STRING("1/2", text); free(text);
    TEST_ASSERT_TRUE(phase_function_range(&f, "0", "1", "-3", "3")); set(f.offset, "0");
    expect(NUMFORGE_PHASE_FOUND, 1);
    f.decreasing = true; TEST_ASSERT_TRUE(phase_function_range(&f, "0", "1", "3", "3"));
    expect(NUMFORGE_PHASE_FOUND, 1); /* first increasing-phase target is 1, at x=2/3 */
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_to_bigdecimal(f.scratch, numforge_phase_locator_x(locator), 12, BIGDECIMAL_ROUND_HALF_EVEN));
    set(f.phase_lower, "0.666666666666"); set(f.phase_upper, "0.666666666667");
    int comparison = 0; TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_compare(&comparison, f.scratch, f.phase_lower)); TEST_ASSERT_TRUE(comparison > 0);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_compare(&comparison, f.scratch, f.phase_upper)); TEST_ASSERT_TRUE(comparison < 0);
}
static void test_no_candidate_does_not_evaluate_or_enumerate(void)
{
    create(); set(f.phase_upper, "0.4"); expect(NUMFORGE_PHASE_NO_CANDIDATE, 0);
    set(f.phase_upper, "0"); expect(NUMFORGE_PHASE_NO_CANDIDATE, 0);
    set(f.phase_lower, "0.4999995"); set(f.phase_upper, "3"); expect(NUMFORGE_PHASE_NO_CANDIDATE, 0);
    TEST_ASSERT_EQUAL_UINT64(0, f.calls);
}
static void test_nonlinear_refinement_limit_and_independent_reference(void)
{
    f.quadratic = true; options.max_evaluations = 1; create(); expect(NUMFORGE_PHASE_EVALUATION_LIMIT, 1);
    options.max_evaluations = 128; options.significant_digits = 64; f.decimal_places = 64; set(f.tolerance, "1E-30"); create();
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, run()); TEST_ASSERT_EQUAL(NUMFORGE_PHASE_FOUND, report.kind);
    TEST_ASSERT_TRUE(report.evaluations > 1 && report.evaluations <= 128);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_sqrt(f.scratch, f.offset, 40, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_to_bigdecimal(f.phase_lower, numforge_phase_locator_x(locator), 40, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_sub(f.scratch, f.scratch, f.phase_lower));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_abs(f.scratch, f.scratch)); set(f.phase_upper, "1E-28");
    int comparison = 1; TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_compare(&comparison, f.scratch, f.phase_upper)); TEST_ASSERT_TRUE(comparison <= 0);
}
static void test_thousand_digit_coordinate_offset_and_span(void)
{
    f.decimal_places = options.significant_digits = 1010;
    TEST_ASSERT_TRUE(phase_function_range(&f, "1E1000", "1E-1000", "0", "3")); set(f.tolerance, "1E-1000"); create(); expect(NUMFORGE_PHASE_FOUND, 1);
    int comparison = 0;
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_compare(&comparison, numforge_phase_locator_x(locator), f.lower)); TEST_ASSERT_TRUE(comparison > 0);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_compare(&comparison, numforge_phase_locator_x(locator), f.upper)); TEST_ASSERT_TRUE(comparison < 0);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_sub(f.temporary, numforge_phase_locator_x(locator), f.lower));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_div(f.temporary, f.temporary, f.span));
    TEST_ASSERT_TRUE(phase_function_set(&f, f.factor, "6")); TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_mul(f.temporary, f.temporary, f.factor));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_to_bigdecimal(f.scratch, f.temporary, 1000, BIGDECIMAL_ROUND_HALF_EVEN));
    set(f.phase_lower, "1"); TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_sub(f.scratch, f.scratch, f.phase_lower));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_abs(f.scratch, f.scratch)); set(f.phase_upper, "1E-1000");
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_compare(&comparison, f.scratch, f.phase_upper)); TEST_ASSERT_TRUE(comparison <= 0);
}
static void test_precision_guard_rejects_endpoint_and_outside_proposals(void)
{
    options.significant_digits = 1; set(f.offset, "0.99"); create(); expect(NUMFORGE_PHASE_PRECISION_LIMIT, 0);
    set(f.period, "0.1"); set(f.offset, "0"); set(f.phase_lower, "1.14"); set(f.phase_upper, "1.25"); create();
    expect(NUMFORGE_PHASE_PRECISION_LIMIT, 0);
}
static void test_unusable_failure_views_and_validation(void)
{
    create(); unusable = true; expect(NUMFORGE_PHASE_UNUSABLE, 1); unusable = false; expect(NUMFORGE_PHASE_FOUND, 1);
    failure = NUMFORGE_NUMERICAL_CALLBACK_FAILED; report = (NumForgePhaseReport){(NumForgePhaseKind)99, 77};
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_CALLBACK_FAILED, run()); preserved(); TEST_ASSERT_NULL(numforge_phase_locator_x(locator));
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_NULL_ARGUMENT, numforge_phase_locate(&report, locator, NULL, f.upper, f.phase_lower, f.phase_upper, NULL, evaluate, &f)); preserved();
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, numforge_phase_locate(&report, locator, f.upper, f.lower, f.phase_lower, f.phase_upper, NULL, evaluate, &f)); preserved();
    NumForgePhaseLocator *old = locator;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_NULL_ARGUMENT, numforge_phase_locator_create(NULL, &options));
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_NULL_ARGUMENT, numforge_phase_locator_create(&locator, NULL));
    set(f.period, "0"); TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, numforge_phase_locator_create(&locator, &options));
    set(f.period, "1"); options.significant_digits = 0; TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, numforge_phase_locator_create(&locator, &options));
    options.significant_digits = 34; options.max_evaluations = 0;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, numforge_phase_locator_create(&locator, &options)); TEST_ASSERT_EQUAL_PTR(old, locator);
    TEST_ASSERT_NULL(numforge_phase_locator_x(NULL)); TEST_ASSERT_NULL(numforge_phase_locator_target(NULL)); numforge_phase_locator_destroy(NULL); numforge_phase_options_init(NULL);
}
static void test_reserve_and_fatal_budget_distinction(void)
{
    options.reserve_bytes = SIZE_MAX; create();
    TEST_ASSERT_TRUE(numforge_budget_begin(UINT64_MAX, 1024U*1024U, SIZE_MAX)); expect(NUMFORGE_PHASE_RESOURCE_LIMIT, 0);
    TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_OK, numforge_budget_failure()); TEST_ASSERT_FALSE(numforge_budget_begin(UINT64_MAX, SIZE_MAX, SIZE_MAX)); numforge_budget_end();
    options.reserve_bytes = 0; create(); expire = true; failure = NUMFORGE_NUMERICAL_CALLBACK_FAILED;
    report = (NumForgePhaseReport){(NumForgePhaseKind)99, 77};
    TEST_ASSERT_TRUE(numforge_budget_begin(UINT64_MAX, SIZE_MAX, SIZE_MAX));
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OUT_OF_MEMORY, run()); preserved(); TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_TIME, numforge_budget_failure());
    numforge_test_budget_expire_after(0); numforge_budget_end(); expire = false; failure = NUMFORGE_NUMERICAL_OK; expect(NUMFORGE_PHASE_FOUND, 1);
}
static void test_every_constructor_and_search_allocation_failure(void)
{
    create(); NumForgePhaseLocator *attempt = NULL;
    numforge_test_allocator_begin(0); TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, numforge_phase_locator_create(&attempt, &options));
    size_t count = numforge_test_allocator_call_count(); numforge_test_allocator_end(); numforge_phase_locator_destroy(attempt);
    for (size_t i = 1; i <= count; ++i) {
        attempt = locator; numforge_test_allocator_begin(i); NumForgeNumericalStatus status = numforge_phase_locator_create(&attempt, &options); numforge_test_allocator_end();
        TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OUT_OF_MEMORY, status); TEST_ASSERT_EQUAL_PTR(locator, attempt);
    }
    numforge_test_allocator_begin(0); TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, run()); count = numforge_test_allocator_call_count(); numforge_test_allocator_end();
    TEST_ASSERT_GREATER_THAN(0, count);
    for (size_t i = 1; i <= count; ++i) {
        create(); report = (NumForgePhaseReport){(NumForgePhaseKind)99, 77};
        numforge_test_allocator_begin(i); NumForgeNumericalStatus status = run(); numforge_test_allocator_end();
        TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OUT_OF_MEMORY, status); preserved(); TEST_ASSERT_NULL(numforge_phase_locator_x(locator)); expect(NUMFORGE_PHASE_FOUND, 1);
    }
}
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_linear_phase_offsets_directions_and_owned_policy);
    RUN_TEST(test_no_candidate_does_not_evaluate_or_enumerate);
    RUN_TEST(test_nonlinear_refinement_limit_and_independent_reference);
    RUN_TEST(test_thousand_digit_coordinate_offset_and_span);
    RUN_TEST(test_precision_guard_rejects_endpoint_and_outside_proposals);
    RUN_TEST(test_unusable_failure_views_and_validation);
    RUN_TEST(test_reserve_and_fatal_budget_distinction);
    RUN_TEST(test_every_constructor_and_search_allocation_failure);
    return UNITY_END();
}
