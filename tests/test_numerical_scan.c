#include <numforge/numerical.h>
#include <numforge/runtime.h>
#include "examples/sdk/numerical_function.h"
#include "src/runtime/numforge_alloc.h"
#include <unity.h>
#include <stdlib.h>
enum { QUADRATIC, CUBIC, CONTACT, MINIMUM, POLE, ZERO, APPROXIMATE_ZERO };
static struct Function {
    NumericalFunction base;
    BigRational *center, *one, *temporary, *epsilon;
    unsigned mode;
    size_t calls, fail_at;
    bool bad_proof;
} f;
static NumForgeRootScan *scan;
static NumForgeRootScanOptions options;
static BigRational *lower, *upper;
void setUp(void)
{
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_TRUE(numforge_alloc_stats_track(true));
#endif
    TEST_ASSERT_TRUE(numerical_function_init(&f.base));
    f.center = bigrational_create(); f.one = bigrational_create(); f.temporary = bigrational_create(); f.epsilon = bigrational_create();
    lower = bigrational_create(); upper = bigrational_create();
    TEST_ASSERT_NOT_NULL(f.center); TEST_ASSERT_NOT_NULL(f.one); TEST_ASSERT_NOT_NULL(f.temporary);
    TEST_ASSERT_NOT_NULL(f.epsilon); TEST_ASSERT_NOT_NULL(lower); TEST_ASSERT_NOT_NULL(upper);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_div(f.one, f.base.two, f.base.two));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_negate(lower, f.base.two));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_copy(upper, f.base.two));
    f.mode = QUADRATIC; f.calls = f.fail_at = 0; f.bad_proof = false;
    numforge_root_scan_options_init(&options);
}
void tearDown(void)
{
    numforge_test_allocator_end(); numforge_test_budget_expire_after(0); numforge_budget_end();
    numforge_root_scan_destroy(scan); scan = NULL; numerical_function_destroy(&f.base);
    bigrational_destroy(f.center); bigrational_destroy(f.one); bigrational_destroy(f.temporary); bigrational_destroy(f.epsilon);
    bigrational_destroy(lower); bigrational_destroy(upper);
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_TRUE(numforge_alloc_stats_complete()); TEST_ASSERT_EQUAL_UINT64(0, numforge_alloc_stats_live());
    TEST_ASSERT_TRUE(numforge_alloc_stats_track(false));
#endif
}
static void set(BigRational *value, const char *text)
{
    BigDecimal *decimal = bigdecimal_create(); TEST_ASSERT_NOT_NULL(decimal);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(decimal, text));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_from_bigdecimal(value, decimal)); bigdecimal_destroy(decimal);
}
static NumForgeNumericalStatus evaluate(BigDecimal *value, BigRational *proposal,
    NumForgeRootSample *sample, const BigRational *x, int64_t digits, void *user)
{
    struct Function *state = user; ++state->calls;
    if (state->fail_at && state->calls == state->fail_at) return NUMFORGE_NUMERICAL_CALLBACK_FAILED;
    BigRational *v = state->base.value; BigRationalStatus status = BIGRATIONAL_OK; int sign = 1;
    if (state->mode == QUADRATIC) {
        return numerical_function_scan_evaluate(value, proposal, sample, x, digits, &state->base);
    }
    if (state->mode == CUBIC) {
        status = bigrational_mul(v, x, x);
        if (status == BIGRATIONAL_OK) status = bigrational_sub(v, v, state->one);
        if (status == BIGRATIONAL_OK) status = bigrational_mul(v, v, x);
    } else if (state->mode == ZERO) status = bigrational_copy(v, state->base.zero);
    else {
        status = bigrational_sub(v, x, state->center);
        if (state->mode == POLE) {
            if (status == BIGRATIONAL_OK) status = bigrational_compare(&sign, v, state->base.zero);
            if (status == BIGRATIONAL_OK && !sign) return NUMFORGE_NUMERICAL_OK;
            if (status == BIGRATIONAL_OK) status = bigrational_div(v, state->one, v);
        } else if (state->mode != APPROXIMATE_ZERO && status == BIGRATIONAL_OK) status = bigrational_mul(v, v, v);
        if (state->mode == MINIMUM && status == BIGRATIONAL_OK) status = bigrational_add(v, v, state->epsilon);
    }
    if (status == BIGRATIONAL_OK) status = bigrational_compare(&sign, v, state->base.zero);
    if (status == BIGRATIONAL_OK) status = bigrational_to_bigdecimal(value, v, digits, BIGDECIMAL_ROUND_HALF_EVEN);
    if (status == BIGRATIONAL_OK && proposal) status = bigrational_copy(proposal, state->bad_proof ? state->one : v);
    sample->real = true; sample->exact_zero = !sign && state->mode != APPROXIMATE_ZERO;
    return status == BIGRATIONAL_OK ? NUMFORGE_NUMERICAL_OK : NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
}
static void run(NumForgeRootDomain domain, size_t count)
{
    numforge_root_scan_destroy(scan); scan = NULL; f.calls = 0;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, numforge_roots_scan(&scan, evaluate, domain, &f, lower, upper, &options));
    TEST_ASSERT_EQUAL_UINT64(count, numforge_root_scan_count(scan));
    NumForgeRootScanStats stats; numforge_root_scan_stats(scan, &stats);
    TEST_ASSERT_EQUAL_UINT64(f.calls, stats.evaluations);
    TEST_ASSERT_TRUE(stats.evaluations <= options.max_evaluations);
    TEST_ASSERT_TRUE(stats.scanned_segments <= options.segments);
    for (size_t i = 1; i < count; ++i) {
        int order = 0;
        TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_compare(&order, numforge_root_x(numforge_root_scan_root(scan, i-1)),
            numforge_root_x(numforge_root_scan_root(scan, i)))); TEST_ASSERT_TRUE(order < 0);
    }
}
static void equals(const BigRational *a, const BigRational *b)
{
    int order = 1; TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_compare(&order, a, b)); TEST_ASSERT_EQUAL_INT(0, order);
}
static void test_exact_shared_endpoints_and_zero_policy(void)
{
    f.mode = CUBIC; run(NULL, 3);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_negate(f.temporary, f.one));
    equals(numforge_root_x(numforge_root_scan_root(scan, 0)), f.temporary);
    equals(numforge_root_x(numforge_root_scan_root(scan, 1)), f.base.zero);
    equals(numforge_root_x(numforge_root_scan_root(scan, 2)), f.one);
    TEST_ASSERT_EQUAL(NUMFORGE_SCAN_FINISHED, numforge_root_scan_stop(scan));
}
static void test_approximate_roots_reuse_single_kernel(void)
{
    run(numerical_function_domain, 2);
    BigDecimal *actual = bigdecimal_create(), *expected = bigdecimal_create(), *tolerance = bigdecimal_create();
    TEST_ASSERT_NOT_NULL(actual); TEST_ASSERT_NOT_NULL(expected); TEST_ASSERT_NOT_NULL(tolerance);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(expected, "2"));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_sqrt(expected, expected, 40, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(tolerance, "1E-12"));
    for (size_t i = 0; i < 2; ++i) {
        const NumForgeRoot *root = numforge_root_scan_root(scan, i); int order = 1;
        TEST_ASSERT_FALSE(numforge_root_exact_zero(root));
        TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_to_bigdecimal(actual, numforge_root_x(root), 40, BIGDECIMAL_ROUND_HALF_EVEN));
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_abs(actual, actual));
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_sub(actual, actual, expected)); TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_abs(actual, actual));
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_compare(&order, actual, tolerance)); TEST_ASSERT_TRUE(order <= 0);
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_compare(&order, numforge_root_residual(root), tolerance)); TEST_ASSERT_TRUE(order <= 0);
    }
    bigdecimal_destroy(actual); bigdecimal_destroy(expected); bigdecimal_destroy(tolerance);
    run(NULL, 0); NumForgeRootScanStats stats; numforge_root_scan_stats(scan, &stats); TEST_ASSERT_TRUE(stats.rejected_brackets > 0);
    f.mode = POLE; run(NULL, 0); numforge_root_scan_stats(scan, &stats); TEST_ASSERT_EQUAL_UINT64(2, stats.domain_gaps);
    f.mode = APPROXIMATE_ZERO; run(numerical_function_domain, 1);
    TEST_ASSERT_FALSE(numforge_root_exact_zero(numforge_root_scan_root(scan, 0)));
}
static void test_rational_contacts_and_high_magnitude(void)
{
    f.mode = CONTACT; set(lower, "0"); set(upper, "1");
    set(f.temporary, "3"); TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_div(f.center, f.one, f.temporary));
    run(NULL, 1); equals(f.center, numforge_root_x(numforge_root_scan_root(scan, 0)));
    TEST_ASSERT_TRUE(numforge_root_exact_zero(numforge_root_scan_root(scan, 0)));
    NumForgeRootScanStats stats; numforge_root_scan_stats(scan, &stats); TEST_ASSERT_EQUAL_UINT64(1, stats.candidate_roots);
    f.mode = MINIMUM; set(f.epsilon, "1E-1000"); run(numerical_function_domain, 0);
    f.mode = CONTACT; set(lower, "1E1000"); TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_add(upper, lower, f.one));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_add(f.center, lower, f.center));
    run(NULL, 1); equals(f.center, numforge_root_x(numforge_root_scan_root(scan, 0)));
    set(lower, "0"); set(upper, "1E-1000"); set(f.center, "1E-1001");
    run(NULL, 1); equals(f.center, numforge_root_x(numforge_root_scan_root(scan, 0)));
}
static void test_explicit_limits_and_diagnostic_getters(void)
{
    f.mode = ZERO; options.max_roots = 3; run(NULL, 3);
    TEST_ASSERT_EQUAL(NUMFORGE_SCAN_ROOT_LIMIT, numforge_root_scan_stop(scan));
    options.max_roots = 64; run(NULL, 64);
    TEST_ASSERT_EQUAL(NUMFORGE_SCAN_ROOT_LIMIT, numforge_root_scan_stop(scan));
    options.max_roots = 32; options.max_evaluations = 2; run(NULL, 2);
    TEST_ASSERT_EQUAL(NUMFORGE_SCAN_EVALUATION_LIMIT, numforge_root_scan_stop(scan));
    f.mode = QUADRATIC; options.segments = 1; options.max_evaluations = options.root_options.max_evaluations;
    run(numerical_function_domain, 0); TEST_ASSERT_EQUAL(NUMFORGE_SCAN_EVALUATION_LIMIT, numforge_root_scan_stop(scan));
    NumForgeRootScanStats stats; numforge_root_scan_stats(NULL, &stats); TEST_ASSERT_EQUAL_UINT64(0, stats.evaluations);
    numforge_root_scan_stats(scan, NULL); TEST_ASSERT_NULL(numforge_root_scan_root(scan, SIZE_MAX));
    TEST_ASSERT_EQUAL_UINT64(0, numforge_root_scan_count(NULL)); TEST_ASSERT_NULL(numforge_root_scan_root(NULL, 0));
    numforge_root_scan_destroy(NULL); numforge_root_scan_options_init(NULL);
}
static void test_atomic_invalid_callbacks_and_options(void)
{
    f.mode = CUBIC; run(NULL, 3); NumForgeRootScan *saved = scan;
    f.mode = ZERO; f.calls = 0; f.fail_at = 4;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_CALLBACK_FAILED, numforge_roots_scan(&scan, evaluate, NULL, &f, lower, upper, &options));
    TEST_ASSERT_EQUAL_PTR(saved, scan); f.fail_at = 0; f.bad_proof = true;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, numforge_roots_scan(&scan, evaluate, NULL, &f, lower, upper, &options));
    f.bad_proof = false; options.segments = 0;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, numforge_roots_scan(&scan, evaluate, NULL, &f, lower, upper, &options));
    numforge_root_scan_options_init(&options); options.max_roots = SIZE_MAX;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_VALUE_TOO_LARGE, numforge_roots_scan(&scan, evaluate, NULL, &f, lower, upper, &options));
    numforge_root_scan_options_init(&options); options.root_options.x_tolerance = f.base.zero;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, numforge_roots_scan(&scan, evaluate, NULL, &f, lower, upper, &options));
    TEST_ASSERT_EQUAL_PTR(saved, scan);
}
static void test_allocation_failures_and_outer_budget(void)
{
    f.mode = CUBIC; options.segments = 4; run(NULL, 3); NumForgeRootScan *saved = scan;
    numforge_test_allocator_begin(0);
    NumForgeRootScan *attempt = NULL;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, numforge_roots_scan(&attempt, evaluate, NULL, &f, lower, upper, &options));
    size_t calls = numforge_test_allocator_call_count(); numforge_test_allocator_end(); numforge_root_scan_destroy(attempt);
    for (size_t i = 1; i <= 160; ++i) {
        size_t failure = i <= 80 ? i : 1+(calls-1)*(i-80)/80;
        numforge_test_allocator_begin(failure);
        TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OUT_OF_MEMORY, numforge_roots_scan(&scan, evaluate, NULL, &f, lower, upper, &options));
        numforge_test_allocator_end(); TEST_ASSERT_EQUAL_PTR(saved, scan);
    }
    for (unsigned kind = 0; kind < 3; ++kind) {
        TEST_ASSERT_TRUE(numforge_budget_begin(5000, kind == 0 ? 0 : SIZE_MAX, kind == 1 ? 0 : SIZE_MAX));
        if (kind == 2) numforge_test_budget_expire_after(100);
        TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OUT_OF_MEMORY, numforge_roots_scan(&scan, evaluate, NULL, &f, lower, upper, &options));
        TEST_ASSERT_EQUAL_PTR(saved, scan); TEST_ASSERT_FALSE(numforge_budget_begin(5000, SIZE_MAX, SIZE_MAX));
        numforge_budget_end(); numforge_test_budget_expire_after(0);
    }
}
int main(void)
{
    UNITY_BEGIN(); RUN_TEST(test_exact_shared_endpoints_and_zero_policy);
    RUN_TEST(test_approximate_roots_reuse_single_kernel); RUN_TEST(test_rational_contacts_and_high_magnitude);
    RUN_TEST(test_explicit_limits_and_diagnostic_getters); RUN_TEST(test_atomic_invalid_callbacks_and_options);
    RUN_TEST(test_allocation_failures_and_outer_budget); return UNITY_END();
}
