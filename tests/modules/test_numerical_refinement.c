#include <numforge/numerical.h>
#include <numforge/runtime.h>
#include "src/runtime/numforge_alloc.h"
#include "examples/sdk/refinement_function.h"
#include <unity.h>
#include <stdlib.h>
#include <string.h>

enum { SPLIT, ACCEPT_AT_TWO, MIXED, ACCEPT, BAD_DECISION };
static struct State {
    unsigned mode;
    size_t count, calls, fail_at, allocate;
    size_t left[64], depth[64];
    unsigned coordinates[65];
    bool expire;
} state;
static NumForgeRefinementOptions options;
static NumForgeRefinementStats stats;
static RefinementFunction function;
void setUp(void)
{
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_TRUE(numforge_alloc_stats_track(true));
#endif
    memset(&state, 0, sizeof(state)); state.count = 3;
    state.coordinates[0] = 0; state.coordinates[1] = 64; state.coordinates[2] = 128;
    numforge_refinement_options_init(&options); options.max_points = 64;
    stats = (NumForgeRefinementStats){91, 92, 93, 94};
}
void tearDown(void)
{
    numforge_test_allocator_end(); numforge_test_budget_expire_after(0); numforge_budget_end();
    refinement_function_destroy(&function);
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_TRUE(numforge_alloc_stats_complete()); TEST_ASSERT_EQUAL_UINT64(0, numforge_alloc_stats_live());
    TEST_ASSERT_TRUE(numforge_alloc_stats_track(false));
#endif
}
static NumForgeNumericalStatus probe(NumForgeRefinementDecision *decision, size_t left, size_t depth, void *user)
{
    TEST_ASSERT_EQUAL_PTR(&state, user); TEST_ASSERT_TRUE(left + 1 < state.count);
    TEST_ASSERT_TRUE(state.calls < 64); TEST_ASSERT_TRUE(state.count < 65);
    state.left[state.calls] = left; state.depth[state.calls] = depth; ++state.calls;
    TEST_ASSERT_EQUAL(NUMFORGE_REFINEMENT_SPLIT, *decision);
    if (state.expire) numforge_test_budget_expire_after(1);
    if (state.fail_at == state.calls) return NUMFORGE_NUMERICAL_VALUE_TOO_LARGE;
    if (state.allocate) {
        void *allocation = numforge_malloc(state.allocate);
        if (!allocation) return NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
        free(allocation);
    }
    unsigned middle = (state.coordinates[left] + state.coordinates[left + 1]) / 2;
    TEST_ASSERT_TRUE(middle > state.coordinates[left] && middle < state.coordinates[left + 1]);
    memmove(state.coordinates + left + 2, state.coordinates + left + 1,
        (state.count - left - 1) * sizeof(*state.coordinates));
    state.coordinates[left + 1] = middle; ++state.count;
    if (state.mode == ACCEPT || (state.mode == ACCEPT_AT_TWO && depth == 2)) *decision = NUMFORGE_REFINEMENT_ACCEPT;
    if (state.mode == MIXED) {
        if (state.calls == 1 || state.calls == 4) *decision = NUMFORGE_REFINEMENT_SKIP;
        else if (state.calls == 3) *decision = NUMFORGE_REFINEMENT_ACCEPT;
    }
    if (state.mode == BAD_DECISION) *decision = (NumForgeRefinementDecision)99;
    return NUMFORGE_NUMERICAL_OK;
}
static NumForgeNumericalStatus run(void)
{ return numforge_refine_intervals(&stats, 3, &options, probe, &state); }
static void preserved(void)
{
    TEST_ASSERT_EQUAL_UINT64(91, stats.points); TEST_ASSERT_EQUAL_UINT64(92, stats.probes);
    TEST_ASSERT_EQUAL_UINT64(93, stats.unresolved_intervals); TEST_ASSERT_EQUAL_UINT(94, stats.limits);
}
static void test_breadth_first_order_and_shifted_indices(void)
{
    state.mode = ACCEPT_AT_TWO;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, run());
    const size_t left[] = {0, 2, 0, 2, 4, 6, 0, 2, 4, 6, 8, 10, 12, 14};
    const size_t depth[] = {0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2};
    TEST_ASSERT_EQUAL_UINT64(14, state.calls); TEST_ASSERT_EQUAL_UINT64(17, state.count);
    for (size_t i = 0; i < state.calls; ++i) {
        TEST_ASSERT_EQUAL_UINT64(left[i], state.left[i]); TEST_ASSERT_EQUAL_UINT64(depth[i], state.depth[i]);
    }
    for (size_t i = 0; i < state.count; ++i) TEST_ASSERT_EQUAL_UINT(i * 8, state.coordinates[i]);
    TEST_ASSERT_EQUAL_UINT64(state.count, stats.points); TEST_ASSERT_EQUAL_UINT64(state.calls, stats.probes);
    TEST_ASSERT_EQUAL_UINT64(0, stats.unresolved_intervals); TEST_ASSERT_EQUAL_UINT(0, stats.limits);
}
static void test_accept_and_skip_drain_only_their_children(void)
{
    state.mode = MIXED;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, run());
    const size_t left[] = {0, 2, 2, 4};
    TEST_ASSERT_EQUAL_UINT64(4, state.calls); TEST_ASSERT_EQUAL_UINT64(7, stats.points);
    for (size_t i = 0; i < state.calls; ++i) TEST_ASSERT_EQUAL_UINT64(left[i], state.left[i]);
    TEST_ASSERT_EQUAL_UINT(0, stats.limits); TEST_ASSERT_EQUAL_UINT64(0, stats.unresolved_intervals);
}
static void test_point_and_depth_limits(void)
{
    options.max_points = 10;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, run());
    TEST_ASSERT_EQUAL_UINT64(10, stats.points); TEST_ASSERT_EQUAL_UINT64(7, stats.probes);
    TEST_ASSERT_EQUAL_UINT64(9, stats.unresolved_intervals); TEST_ASSERT_EQUAL_UINT(NUMFORGE_REFINEMENT_POINT_LIMIT, stats.limits);
    state.calls = 0; options.max_points = 3;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, run()); TEST_ASSERT_EQUAL_UINT64(0, state.calls);
    TEST_ASSERT_EQUAL_UINT64(2, stats.unresolved_intervals);
}
static void test_depth_limit_and_zero_depth(void)
{
    options.max_depth = 1;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, run());
    TEST_ASSERT_EQUAL_UINT64(5, stats.points); TEST_ASSERT_EQUAL_UINT64(2, stats.probes);
    TEST_ASSERT_EQUAL_UINT64(4, stats.unresolved_intervals); TEST_ASSERT_EQUAL_UINT(NUMFORGE_REFINEMENT_DEPTH_LIMIT, stats.limits);
    state.calls = 0; options.max_depth = 0;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, run()); TEST_ASSERT_EQUAL_UINT64(0, state.calls);
    TEST_ASSERT_EQUAL_UINT64(2, stats.unresolved_intervals); TEST_ASSERT_EQUAL_UINT(NUMFORGE_REFINEMENT_DEPTH_LIMIT, stats.limits);
}
static void test_reserve_and_observed_cost_stop_without_failure(void)
{
    /* Leave room for one probe, then reject the observed cost on both 32/64-bit queues. */
    options.max_points = 16; options.reserve_bytes = 1000; state.allocate = 512;
    TEST_ASSERT_TRUE(numforge_budget_begin(UINT64_MAX, 2000, SIZE_MAX));
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, run()); TEST_ASSERT_EQUAL_UINT64(1, state.calls);
    TEST_ASSERT_EQUAL_UINT64(4, stats.points); TEST_ASSERT_EQUAL_UINT64(3, stats.unresolved_intervals);
    TEST_ASSERT_EQUAL_UINT(NUMFORGE_REFINEMENT_RESOURCE_LIMIT, stats.limits);
    TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_OK, numforge_budget_failure());
    TEST_ASSERT_TRUE(numforge_budget_available_bytes() >= options.reserve_bytes);
    TEST_ASSERT_FALSE(numforge_budget_begin(UINT64_MAX, SIZE_MAX, SIZE_MAX)); numforge_budget_end();
    state.calls = 0;
    TEST_ASSERT_TRUE(numforge_budget_begin(UINT64_MAX, options.reserve_bytes + 1, SIZE_MAX));
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, run()); TEST_ASSERT_EQUAL_UINT64(0, state.calls);
    TEST_ASSERT_EQUAL_UINT(NUMFORGE_REFINEMENT_RESOURCE_LIMIT, stats.limits);
    TEST_ASSERT_EQUAL_UINT64(options.reserve_bytes + 1, numforge_budget_available_bytes()); numforge_budget_end();
    state.calls = 0; options.reserve_ms = UINT64_MAX;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, run()); TEST_ASSERT_EQUAL_UINT64(0, state.calls);
    TEST_ASSERT_EQUAL_UINT(NUMFORGE_REFINEMENT_RESOURCE_LIMIT, stats.limits);
}
static void test_fatal_callbacks_and_budget_preserve_stats(void)
{
    state.fail_at = 2;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_VALUE_TOO_LARGE, run()); preserved();
    TEST_ASSERT_EQUAL_UINT64(2, state.calls); TEST_ASSERT_EQUAL_UINT64(4, state.count);
    state.calls = 0; state.mode = BAD_DECISION; state.fail_at = 0;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, run()); preserved();
    state.calls = 0; state.expire = true; state.fail_at = 1;
    TEST_ASSERT_TRUE(numforge_budget_begin(UINT64_MAX, SIZE_MAX, SIZE_MAX));
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OUT_OF_MEMORY, run()); preserved();
    TEST_ASSERT_EQUAL_UINT64(1, state.calls); TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_TIME, numforge_budget_failure());
    numforge_test_budget_expire_after(0); numforge_budget_end(); state.calls = 0;
    TEST_ASSERT_TRUE(numforge_budget_begin(0, SIZE_MAX, SIZE_MAX));
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OUT_OF_MEMORY, run()); preserved(); TEST_ASSERT_EQUAL_UINT64(0, state.calls);
}
static void test_allocation_failure_and_validation(void)
{
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_NULL_ARGUMENT, numforge_refine_intervals(NULL, 3, &options, probe, &state));
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_NULL_ARGUMENT, numforge_refine_intervals(&stats, 3, &options, NULL, &state));
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, numforge_refine_intervals(&stats, 1, &options, probe, &state));
    options.max_points = 2; TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, run());
    options.max_points = SIZE_MAX; TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, run()); preserved();
    options.max_points = 16; state.mode = ACCEPT; state.allocate = 16;
    for (size_t failure = 1; failure <= 4; ++failure) {
        state.calls = 0; state.count = 3;
        state.coordinates[0] = 0; state.coordinates[1] = 64; state.coordinates[2] = 128;
        numforge_test_allocator_begin(failure); NumForgeNumericalStatus status = run(); numforge_test_allocator_end();
        if (failure <= 3) { TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OUT_OF_MEMORY, status); preserved(); }
        else { TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, status); TEST_ASSERT_EQUAL_UINT64(5, stats.points); }
    }
}
static void test_exact_refinement_with_thousand_digit_offset_and_tiny_span(void)
{
    TEST_ASSERT_TRUE(refinement_function_init(&function, "1E1000", "1E-1000", "1E-2002"));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_add(function.points[1], function.points[0], function.points[1]));
    const BigRational *lower = function.points[0], *upper = function.points[1];
    options.max_points = REFINEMENT_EXAMPLE_CAPACITY;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, numforge_refine_intervals(&stats, 2, &options, refinement_function_probe, &function));
    TEST_ASSERT_EQUAL_UINT64(17, stats.points); TEST_ASSERT_EQUAL_UINT64(15, stats.probes);
    TEST_ASSERT_EQUAL_UINT(0, stats.limits); TEST_ASSERT_EQUAL_UINT64(0, stats.unresolved_intervals);
    TEST_ASSERT_EQUAL_PTR(lower, function.points[0]); TEST_ASSERT_EQUAL_PTR(upper, function.points[16]);
    BigDecimal *number = bigdecimal_create(); TEST_ASSERT_NOT_NULL(number);
    TEST_ASSERT_TRUE(refinement_function_set(function.delta, number, "1E-1000"));
    TEST_ASSERT_TRUE(refinement_function_set(function.chord, number, "16")); bigdecimal_destroy(number);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_div(function.delta, function.delta, function.chord));
    for (size_t i = 1; i < function.count; ++i) {
        int comparison = 1;
        TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_sub(function.value, function.points[i], function.points[i - 1]));
        TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_compare(&comparison, function.value, function.delta));
        TEST_ASSERT_EQUAL_INT(0, comparison);
    }
}
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_breadth_first_order_and_shifted_indices);
    RUN_TEST(test_accept_and_skip_drain_only_their_children);
    RUN_TEST(test_point_and_depth_limits);
    RUN_TEST(test_depth_limit_and_zero_depth);
    RUN_TEST(test_reserve_and_observed_cost_stop_without_failure);
    RUN_TEST(test_fatal_callbacks_and_budget_preserve_stats);
    RUN_TEST(test_allocation_failure_and_validation);
    RUN_TEST(test_exact_refinement_with_thousand_digit_offset_and_tiny_span);
    return UNITY_END();
}
