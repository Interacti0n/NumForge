#include <numforge/numerical.h>
#include <numforge/runtime.h>
#include "src/runtime/numforge_alloc.h"
#include "tests/support/numbers.h"

static BigRational *lower, *upper, *scratch, *tolerance;
static NumForgeIntegral *integral;
static size_t calls, stop_at;
static unsigned power;
static bool gap, bad_flags, expire;
void setUp(void)
{
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_TRUE(numforge_alloc_stats_track(true));
#endif
    lower = test_fraction("0", "1"); upper = test_fraction("1", "1");
    scratch = test_fraction("0", "1"); tolerance = test_fraction("1", "1000000");
    integral = NULL; calls = stop_at = 0; power = 2; gap = bad_flags = expire = false;
}
void tearDown(void)
{
    numforge_test_allocator_end(); numforge_test_budget_expire_after(0); numforge_budget_end();
    numforge_integral_destroy(integral);
    bigrational_destroy(lower); bigrational_destroy(upper); bigrational_destroy(scratch); bigrational_destroy(tolerance);
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_TRUE(numforge_alloc_stats_complete()); TEST_ASSERT_EQUAL_UINT64(0, numforge_alloc_stats_live());
    TEST_ASSERT_TRUE(numforge_alloc_stats_track(false));
#endif
}
static NumForgeNumericalStatus polynomial(BigDecimal *value, NumForgeRootSample *flags,
    const BigRational *x, int64_t places, void *user)
{
    (void)user; ++calls;
    if (expire) numforge_test_budget_expire_after(1);
    if (stop_at && calls == stop_at) {
        if (gap) return NUMFORGE_NUMERICAL_OK;
        return NUMFORGE_NUMERICAL_CALLBACK_FAILED;
    }
    BigRationalStatus status = bigrational_copy(scratch, power ? x : upper);
    for (unsigned i = 1; status == BIGRATIONAL_OK && i < power; ++i)
        status = bigrational_mul(scratch, scratch, x);
    if (status == BIGRATIONAL_OK) status = bigrational_to_bigdecimal(value, scratch, places, BIGDECIMAL_ROUND_HALF_EVEN);
    flags->real = true; flags->exact_zero = bad_flags;
    return status == BIGRATIONAL_OK ? NUMFORGE_NUMERICAL_OK : NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
}
static void test_polynomials_and_error_estimate(void)
{
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, numforge_integrate(&integral, polynomial, NULL, lower, upper, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_INTEGRAL_ESTIMATED, numforge_integral_kind(integral));
    test_assert_fraction("1/3", numforge_integral_value(integral)); test_assert_fraction("0", numforge_integral_error(integral));
    TEST_ASSERT_EQUAL_UINT64(5, calls); TEST_ASSERT_EQUAL_UINT64(calls, numforge_integral_evaluations(integral));
    TEST_ASSERT_EQUAL_UINT64(2, numforge_integral_refinements(integral));
    numforge_integral_destroy(integral); integral = NULL; calls = 0; power = 4;
    NumForgeIntegralOptions options; numforge_integral_options_init(&options);
    options.max_refinements = 2;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, numforge_integrate(&integral, polynomial, NULL, lower, upper, &options));
    TEST_ASSERT_EQUAL(NUMFORGE_INTEGRAL_REFINEMENT_LIMIT, numforge_integral_kind(integral));
    /* Independent antiderivative 1/5; Simpson's quartic error is 1/(120*n^4). */
    test_assert_fraction("77/384", numforge_integral_value(integral));
    test_assert_fraction("1/1920", numforge_integral_error(integral));
    numforge_integral_destroy(integral); integral = NULL; calls = 0;
    options.max_refinements = 10; options.absolute_tolerance = tolerance;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, numforge_integrate(&integral, polynomial, NULL, lower, upper, &options));
    TEST_ASSERT_EQUAL(NUMFORGE_INTEGRAL_ESTIMATED, numforge_integral_kind(integral));
    TEST_ASSERT_EQUAL_UINT64(33, calls);
    test_assert_fraction("1/7864320", numforge_integral_error(integral));
}
static void test_limits_gaps_and_callback_errors(void)
{
    NumForgeIntegralOptions options; numforge_integral_options_init(&options); options.max_evaluations = 8; power = 4;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, numforge_integrate(&integral, polynomial, NULL, lower, upper, &options));
    TEST_ASSERT_EQUAL(NUMFORGE_INTEGRAL_EVALUATION_LIMIT, numforge_integral_kind(integral));
    TEST_ASSERT_EQUAL_UINT64(5, calls); test_assert_fraction("77/384", numforge_integral_value(integral));
    NumForgeIntegral *saved = integral;
    calls = 0; stop_at = 4;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_CALLBACK_FAILED, numforge_integrate(&integral, polynomial, NULL, lower, upper, NULL));
    TEST_ASSERT_EQUAL_PTR(saved, integral); TEST_ASSERT_EQUAL_UINT64(4, calls);
    calls = 0; gap = true;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, numforge_integrate(&integral, polynomial, NULL, lower, upper, NULL));
    numforge_integral_destroy(saved);
    TEST_ASSERT_EQUAL(NUMFORGE_INTEGRAL_DOMAIN_GAP, numforge_integral_kind(integral));
    TEST_ASSERT_NULL(numforge_integral_value(integral)); TEST_ASSERT_NULL(numforge_integral_error(integral));
    TEST_ASSERT_EQUAL_UINT64(4, numforge_integral_evaluations(integral));
}
static void test_thousand_digit_interval_and_ordinate(void)
{
    BigDecimal *number = test_decimal("1E-1000");
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_from_bigdecimal(upper, number));
    power = 0; /* Constant upper=1E-1000, width=1E-1000. */
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, numforge_integrate(&integral, polynomial, NULL, lower, upper, NULL));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(number, "1E-2000"));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_from_bigdecimal(scratch, number));
    int order = 1; TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_compare(&order, scratch, numforge_integral_value(integral)));
    TEST_ASSERT_EQUAL_INT(0, order); test_assert_fraction("0", numforge_integral_error(integral));
    numforge_integral_destroy(integral); integral = NULL;
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(number, "1E1000"));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_from_bigdecimal(lower, number));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(number, "1"));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_from_bigdecimal(upper, number));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_add(upper, lower, upper));
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, numforge_integrate(&integral, polynomial, NULL, lower, upper, NULL));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_compare(&order, upper, numforge_integral_value(integral)));
    TEST_ASSERT_EQUAL_INT(0, order); bigdecimal_destroy(number);
}
static void test_validation_and_null_queries(void)
{
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_NULL_ARGUMENT, numforge_integrate(NULL, polynomial, NULL, lower, upper, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_NULL_ARGUMENT, numforge_integrate(&integral, NULL, NULL, lower, upper, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_NULL_ARGUMENT, numforge_integrate(&integral, polynomial, NULL, NULL, upper, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_NULL_ARGUMENT, numforge_integrate(&integral, polynomial, NULL, lower, NULL, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, numforge_integrate(&integral, polynomial, NULL, upper, lower, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, numforge_integrate(&integral, polynomial, NULL, upper, upper, NULL));
    NumForgeIntegralOptions options; numforge_integral_options_init(&options);
    for (unsigned mode = 0; mode < 4; ++mode) {
        numforge_integral_options_init(&options);
        if (mode == 0) options.decimal_places = 0;
        if (mode == 1) options.max_refinements = 1;
        if (mode == 2) options.max_evaluations = 4;
        if (mode == 3) options.absolute_tolerance = lower;
        TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, numforge_integrate(&integral, polynomial, NULL, lower, upper, &options));
    }
    TEST_ASSERT_EQUAL_UINT64(0, calls); bad_flags = true;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, numforge_integrate(&integral, polynomial, NULL, lower, upper, NULL));
    TEST_ASSERT_NULL(integral); TEST_ASSERT_NULL(numforge_integral_value(NULL)); TEST_ASSERT_NULL(numforge_integral_error(NULL));
    TEST_ASSERT_EQUAL_UINT64(0, numforge_integral_evaluations(NULL)); TEST_ASSERT_EQUAL_UINT64(0, numforge_integral_refinements(NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_INTEGRAL_EVALUATION_LIMIT, numforge_integral_kind(NULL)); numforge_integral_destroy(NULL);
}
static void test_allocation_failure_and_budget_recovery(void)
{
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, numforge_integrate(&integral, polynomial, NULL, lower, upper, NULL));
    NumForgeIntegral *saved = integral; bool complete = false;
    for (size_t failure = 1; failure < 8192; ++failure) {
        calls = 0; numforge_test_allocator_begin(failure);
        NumForgeNumericalStatus status = numforge_integrate(&integral, polynomial, NULL, lower, upper, NULL);
        bool injected = numforge_test_allocator_did_fail(); numforge_test_allocator_end();
        if (status == NUMFORGE_NUMERICAL_OK) { numforge_integral_destroy(integral); integral = saved; }
        else { TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OUT_OF_MEMORY, status); TEST_ASSERT_EQUAL_PTR(saved, integral); }
        test_assert_fraction("1/3", numforge_integral_value(saved));
        if (!injected) { complete = true; break; }
    }
    TEST_ASSERT_TRUE(complete);
    for (unsigned mode = 0; mode < 3; ++mode) {
        TEST_ASSERT_TRUE(numforge_budget_begin(mode == 0 ? 0 : 5000, mode == 1 ? 0 : SIZE_MAX, SIZE_MAX));
        expire = mode == 2;
        TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OUT_OF_MEMORY, numforge_integrate(&integral, polynomial, NULL, lower, upper, NULL));
        TEST_ASSERT_EQUAL_PTR(saved, integral);
        TEST_ASSERT_FALSE(numforge_budget_begin(5000, SIZE_MAX, SIZE_MAX));
        numforge_test_budget_expire_after(0); numforge_budget_end();
    }
    expire = false;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, numforge_integrate(&integral, polynomial, NULL, lower, upper, NULL));
    numforge_integral_destroy(saved);
}
int main(void)
{
    UNITY_BEGIN(); RUN_TEST(test_polynomials_and_error_estimate); RUN_TEST(test_limits_gaps_and_callback_errors);
    RUN_TEST(test_thousand_digit_interval_and_ordinate); RUN_TEST(test_validation_and_null_queries);
    RUN_TEST(test_allocation_failure_and_budget_recovery); return UNITY_END();
}
