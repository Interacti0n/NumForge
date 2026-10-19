#include <numforge/numerical.h>
#include <numforge/runtime.h>
#include "examples/sdk/numerical_function.h"
#include "src/runtime/numforge_alloc.h"
#include <unity.h>
#include <stdlib.h>
#include <string.h>
static NumericalFunction f;
static BigRational *lower, *upper;
static NumForgeRoot *root;
static NumForgeRootOptions options;
static unsigned mode, calls;
void setUp(void)
{
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_TRUE(numforge_alloc_stats_track(true));
#endif
    TEST_ASSERT_TRUE(numerical_function_init(&f)); lower = bigrational_create(); upper = bigrational_create();
    TEST_ASSERT_NOT_NULL(lower); TEST_ASSERT_NOT_NULL(upper);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_copy(upper, f.two));
    numforge_root_options_init(&options); mode = calls = 0;
}
void tearDown(void)
{
    numforge_test_allocator_end(); numforge_test_budget_expire_after(0); numforge_budget_end();
    numforge_root_destroy(root); root = NULL; bigrational_destroy(lower); bigrational_destroy(upper);
    numerical_function_destroy(&f);
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_TRUE(numforge_alloc_stats_complete()); TEST_ASSERT_EQUAL_UINT64(0, numforge_alloc_stats_live());
    TEST_ASSERT_TRUE(numforge_alloc_stats_track(false));
#endif
}
static void bound(BigRational *value, const char *text)
{
    BigDecimal *decimal = bigdecimal_create(); TEST_ASSERT_NOT_NULL(decimal);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(decimal, text));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_from_bigdecimal(value, decimal)); bigdecimal_destroy(decimal);
}
static NumForgeNumericalStatus evaluate(BigDecimal *value, NumForgeRootSample *sample,
    const BigRational *x, int64_t digits, void *user)
{
    ++calls;
    if (mode == 1 && calls == 3) return NUMFORGE_NUMERICAL_CALLBACK_FAILED;
    if (mode == 6 && digits > options.decimal_places) return NUMFORGE_NUMERICAL_CALLBACK_FAILED;
    if (mode == 2 && calls == 3) return NUMFORGE_NUMERICAL_OK; /* domain gap */
    if (mode == 5) {
        int sign = 0;
        BigRationalStatus status = bigrational_compare(&sign, x, f.zero);
        if (status == BIGRATIONAL_OK) status = bigrational_to_bigdecimal(value, x, digits, BIGDECIMAL_ROUND_HALF_EVEN);
        sample->real = true; sample->exact_zero = sign == 0;
        return status == BIGRATIONAL_OK ? NUMFORGE_NUMERICAL_OK : NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    }
    NumForgeNumericalStatus status = numerical_function_evaluate(value, sample, x, digits, user);
    if (status != NUMFORGE_NUMERICAL_OK) return status;
    if (mode == 3 && digits > options.decimal_places) {
        if (bigdecimal_set_string(value, "1") != BIGDECIMAL_OK) return NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    }
    if (mode == 4) sample->exact_zero = true; /* Contradictory proof must fail. */
    return status;
}
static void solve(NumForgeRootDomain domain)
{
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, numforge_root_solve(&root, evaluate, domain, &f, lower, upper, &options));
    TEST_ASSERT_EQUAL_UINT64(calls, numforge_root_evaluations(root));
    TEST_ASSERT_TRUE(calls <= options.max_evaluations);
    TEST_ASSERT_TRUE(numforge_root_iterations(root) <= options.max_iterations);
}
static void test_independent_reference_and_borrowed_inputs(void)
{
    solve(numerical_function_domain); TEST_ASSERT_EQUAL(NUMFORGE_ROOT_FOUND, numforge_root_kind(root));
    TEST_ASSERT_FALSE(numforge_root_exact_zero(root));
    BigDecimal *actual = bigdecimal_create(), *expected = bigdecimal_create(), *tolerance = bigdecimal_create(); int order = 1;
    TEST_ASSERT_NOT_NULL(actual); TEST_ASSERT_NOT_NULL(expected); TEST_ASSERT_NOT_NULL(tolerance);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_to_bigdecimal(actual, numforge_root_x(root), 30, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(expected, "1.41421356237309504880168872421"));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_sub(actual, actual, expected));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_abs(actual, actual));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(tolerance, "1E-12"));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_compare(&order, actual, tolerance)); TEST_ASSERT_TRUE(order <= 0);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_compare(&order, numforge_root_residual(root), tolerance)); TEST_ASSERT_TRUE(order <= 0);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_compare(&order, upper, f.two)); TEST_ASSERT_EQUAL_INT(0, order);
    bigdecimal_destroy(actual); bigdecimal_destroy(expected); bigdecimal_destroy(tolerance);
}
static void test_exact_samples_and_domain_contract(void)
{
    mode = 5; solve(NULL); TEST_ASSERT_EQUAL(NUMFORGE_ROOT_FOUND, numforge_root_kind(root));
    TEST_ASSERT_TRUE(numforge_root_exact_zero(root)); TEST_ASSERT_EQUAL_UINT(1, calls);
    numforge_root_destroy(root); root = NULL; calls = 0;
    bound(lower, "-1"); bound(upper, "1"); solve(NULL);
    TEST_ASSERT_TRUE(numforge_root_exact_zero(root)); TEST_ASSERT_EQUAL_UINT(3, calls);
    numforge_root_destroy(root); root = NULL; calls = mode = 0;
    bound(lower, "0"); bound(upper, "2");
    /* x*x-2 has no rational zero: without continuity evidence it cannot converge. */
    solve(NULL); TEST_ASSERT_EQUAL(NUMFORGE_ROOT_DOMAIN_UNVERIFIED, numforge_root_kind(root));
    TEST_ASSERT_NULL(numforge_root_x(root)); TEST_ASSERT_NULL(numforge_root_residual(root));
    numforge_root_destroy(root); root = NULL; calls = 0;
    bound(lower, "2"); bound(upper, "3"); solve(numerical_function_domain);
    TEST_ASSERT_EQUAL(NUMFORGE_ROOT_NOT_BRACKETED, numforge_root_kind(root));
    numforge_root_destroy(root); root = NULL; calls = 0;
    bound(lower, "0"); bound(upper, "2"); mode = 2; solve(numerical_function_domain);
    TEST_ASSERT_EQUAL(NUMFORGE_ROOT_DOMAIN_GAP, numforge_root_kind(root));
}
static void test_stability_and_limits(void)
{
    mode = 3; solve(numerical_function_domain); TEST_ASSERT_EQUAL(NUMFORGE_ROOT_PRECISION_LIMIT, numforge_root_kind(root));
    numforge_root_destroy(root); root = NULL; calls = mode = 0; options.max_iterations = 1;
    solve(numerical_function_domain); TEST_ASSERT_EQUAL(NUMFORGE_ROOT_ITERATION_LIMIT, numforge_root_kind(root));
    numforge_root_destroy(root); root = NULL; calls = 0; options.max_evaluations = 4;
    solve(numerical_function_domain); TEST_ASSERT_EQUAL(NUMFORGE_ROOT_ITERATION_LIMIT, numforge_root_kind(root)); TEST_ASSERT_EQUAL_UINT(2, calls);
}
static void test_invalid_and_callback_failures_preserve_output(void)
{
    solve(numerical_function_domain); NumForgeRoot *saved = root;
    mode = 1; calls = 0;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_CALLBACK_FAILED, numforge_root_solve(&root, evaluate, NULL, &f, lower, upper, &options));
    TEST_ASSERT_EQUAL_PTR(saved, root);
    mode = 6; calls = 0;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_CALLBACK_FAILED, numforge_root_solve(&root, evaluate,
        numerical_function_domain, &f, lower, upper, &options));
    TEST_ASSERT_EQUAL_PTR(saved, root);
    mode = 4; calls = 0;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, numforge_root_solve(&root, evaluate, NULL, &f, lower, upper, &options));
    TEST_ASSERT_EQUAL_PTR(saved, root);
    mode = 0; options.decimal_places = INT64_MAX;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, numforge_root_solve(&root, evaluate, NULL, &f, lower, upper, &options));
    numforge_root_options_init(&options); options.x_tolerance = lower;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, numforge_root_solve(&root, evaluate, NULL, &f, lower, upper, &options));
    TEST_ASSERT_EQUAL_PTR(saved, root);
    numforge_root_options_init(&options); options.max_evaluations = 3;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, numforge_root_solve(&root, evaluate, NULL, &f, lower, upper, &options));
    numforge_root_options_init(&options); options.max_iterations = 0;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, numforge_root_solve(&root, evaluate, NULL, &f, lower, upper, &options));
    numforge_root_options_init(&options);
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, numforge_root_solve(&root, evaluate, NULL, &f, upper, upper, &options));
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_NULL_ARGUMENT, numforge_root_solve(&root, NULL, NULL, NULL, lower, upper, NULL));
    TEST_ASSERT_NULL(numforge_root_x(NULL)); TEST_ASSERT_FALSE(numforge_root_exact_zero(NULL));
    numforge_root_destroy(NULL); numforge_root_options_init(NULL);
}
static void test_high_precision_independent_decimal_sqrt(void)
{
    BigDecimal *expected = bigdecimal_create(), *actual = bigdecimal_create(), *tolerance = bigdecimal_create();
    BigRational *width = bigrational_create(); TEST_ASSERT_NOT_NULL(expected); TEST_ASSERT_NOT_NULL(actual);
    TEST_ASSERT_NOT_NULL(tolerance); TEST_ASSERT_NOT_NULL(width);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(tolerance, "1E-1000"));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_from_bigdecimal(width, tolerance));
    options.x_tolerance = width; options.residual_tolerance = tolerance; options.decimal_places = 1020;
    options.max_iterations = 3400; options.max_evaluations = 3410;
    solve(numerical_function_domain); TEST_ASSERT_EQUAL(NUMFORGE_ROOT_FOUND, numforge_root_kind(root));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(expected, "2"));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_sqrt(expected, expected, 1010, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_to_bigdecimal(actual, numforge_root_x(root), 1010, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_sub(actual, actual, expected));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_abs(actual, actual)); int order = 1;
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_compare(&order, actual, tolerance)); TEST_ASSERT_TRUE(order <= 0);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_compare(&order, numforge_root_residual(root), tolerance)); TEST_ASSERT_TRUE(order <= 0);
    bigdecimal_destroy(expected); bigdecimal_destroy(actual); bigdecimal_destroy(tolerance); bigrational_destroy(width);
}
static void test_atomic_allocations_and_inherited_budgets(void)
{
    /* A wide tolerance exercises acceptance/verification with a bounded fault sweep. */
    BigDecimal *tolerance = bigdecimal_create(); TEST_ASSERT_NOT_NULL(tolerance);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(tolerance, "1")); options.residual_tolerance = tolerance;
    options.x_tolerance = f.two; solve(numerical_function_domain); NumForgeRoot *saved = root;
    bool complete = false;
    for (size_t failure = 1; failure < 8192; ++failure) {
        calls = 0; numforge_test_allocator_begin(failure);
        NumForgeNumericalStatus status = numforge_root_solve(&root, evaluate, numerical_function_domain, &f, lower, upper, &options);
        bool injected = numforge_test_allocator_did_fail(); numforge_test_allocator_end();
        if (status == NUMFORGE_NUMERICAL_OK) { numforge_root_destroy(root); root = saved; }
        else { TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OUT_OF_MEMORY, status); TEST_ASSERT_EQUAL_PTR(saved, root); }
        if (!injected) { complete = true; break; }
    }
    TEST_ASSERT_TRUE(complete);
    for (unsigned mode_index = 0; mode_index < 3; ++mode_index) {
        TEST_ASSERT_TRUE(numforge_budget_begin(5000, mode_index == 0 ? 0 : SIZE_MAX, mode_index == 1 ? 0 : SIZE_MAX));
        if (mode_index == 2) numforge_test_budget_expire_after(32);
        TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OUT_OF_MEMORY, numforge_root_solve(&root, evaluate, numerical_function_domain, &f, lower, upper, &options));
        TEST_ASSERT_EQUAL_PTR(saved, root); TEST_ASSERT_FALSE(numforge_budget_begin(5000, SIZE_MAX, SIZE_MAX));
        numforge_budget_end(); numforge_test_budget_expire_after(0);
    }
    bigdecimal_destroy(tolerance);
}
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_independent_reference_and_borrowed_inputs);
    RUN_TEST(test_exact_samples_and_domain_contract);
    RUN_TEST(test_stability_and_limits);
    RUN_TEST(test_invalid_and_callback_failures_preserve_output);
    RUN_TEST(test_high_precision_independent_decimal_sqrt);
    RUN_TEST(test_atomic_allocations_and_inherited_budgets);
    return UNITY_END();
}
