#include "expression.h"
#include "../src/internal/numforge_alloc.h"
#include <unity.h>
#include <stdlib.h>
#include <string.h>

static CalculatorSession session;
static CalculatorContext context;
static ApplicationExpression *prepared;
static CalculatorValue arguments[3];
static ApplicationExpressionSample *samples;
static size_t sample_count;

void setUp(void)
{
    calculator_context_init(&context);
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_TRUE(numforge_alloc_stats_track(true));
#endif
}
void tearDown(void)
{
    numforge_budget_end();
    numforge_test_allocator_end();
    application_expression_samples_destroy(samples, sample_count);
    samples = NULL; sample_count = 0U;
    application_expression_destroy(prepared); prepared = NULL;
    for (size_t i = 0; i < 3U; ++i) calculator_value_destroy(&arguments[i]);
    calculator_session_destroy(&session);
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_TRUE(numforge_alloc_stats_complete());
    TEST_ASSERT_EQUAL_UINT64(0U, numforge_alloc_stats_live());
    TEST_ASSERT_TRUE(numforge_alloc_stats_track(false));
#endif
}
static void assign(uint64_t revision, bool commit, const char *input)
{
    char *text = NULL; CalculatorError error;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_session_compute(&session, revision,
        commit, input, &context, &text, &error, NULL));
    free(text);
}
static void argument(size_t index, const char *input)
{
    CalculatorError error;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute_value(input, &context, &arguments[index], &error));
}
static void prepare(const char *input)
{
    CalculatorError error;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_expression_prepare(&prepared,
        input, "x", &session, &context, &error));
}
static void sample(size_t count)
{
    CalculatorError error;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_expression_sample(prepared,
        arguments, count, &samples, &error));
    sample_count = count;
}
static void text(size_t index, const char *expected)
{
    char *formatted = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, samples[index].error.status);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_format_value(&samples[index].value, &context, &formatted));
    TEST_ASSERT_EQUAL_STRING(expected, formatted); free(formatted);
}

static void test_snapshots_parameter_shadowing_and_session_lifetime(void)
{
    assign(1U, true, "x=99"); assign(2U, true, "y=1/3"); assign(3U, true, "5");
    assign(4U, false, "rand()");
    CalculatorSession before = session;
    char input[] = "x+y+ans+y-y";
    prepare(input); memset(input, '?', sizeof(input) - 1U);
    argument(0U, "1"); argument(1U, "2"); sample(2U);
    text(0U, "19/3"); text(1U, "22/3");
    TEST_ASSERT_EQUAL_MEMORY(&before, &session, sizeof(session));
    application_expression_samples_destroy(samples, sample_count); samples = NULL; sample_count = 0U;
    calculator_session_destroy(&session);
    sample(2U); text(0U, "19/3"); text(1U, "22/3");
}
static void test_typed_complex_and_quantity_parameters(void)
{
    prepare("x*x"); argument(0U, "1+i"); sample(1U); text(0U, "2*i");
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_COMPLEX_RATIONAL, samples[0].value.kind);
    application_expression_samples_destroy(samples, sample_count); samples = NULL; sample_count = 0U;
    application_expression_destroy(prepared); prepared = NULL;
    calculator_value_destroy(&arguments[0]);
    prepare("x+x"); argument(0U, "qty(2;\"m\")"); sample(1U); text(0U, "4 m");
    TEST_ASSERT_TRUE(samples[0].value.quantity);
}
static void test_domain_gaps_do_not_abort_later_points(void)
{
    prepare("1/x"); argument(0U, "-1"); argument(1U, "0"); argument(2U, "1"); sample(3U);
    text(0U, "-1"); text(2U, "1");
    TEST_ASSERT_EQUAL(CALCULATOR_DIVISION_BY_ZERO, samples[1].error.status);
    TEST_ASSERT_NULL(samples[1].value.number);
}
static void test_context_and_unused_snapshots(void)
{
    context.angle_unit = CALCULATOR_ANGLE_DEGREES;
    assign(1U, true, "y=qty(2;\"m\")");
    prepare("sin(x)"); context.angle_unit = CALCULATOR_ANGLE_RADIANS;
    argument(0U, "90"); sample(1U); text(0U, "1");
}
static void test_invalid_inputs_random_aliases_and_error_offsets(void)
{
    prepare("x+1"); CalculatorError error;
    const char *invalid[] = {"x=2", "rand()", "x+random()", "x+missing", "x+ans"};
    for (size_t i = 0; i < sizeof(invalid)/sizeof(invalid[0]); ++i)
    {
        ApplicationExpression *attempt = prepared;
        TEST_ASSERT_NOT_EQUAL(CALCULATOR_OK, application_expression_prepare(&attempt,
            invalid[i], "x", &session, &context, &error));
        TEST_ASSERT_EQUAL_PTR(prepared, attempt);
        TEST_ASSERT_TRUE(error.offset < strlen(invalid[i]));
    }
    const char *names[] = {"", "i", "ans", "sin", "e", "x1", "x_y"};
    for (size_t i = 0; i < sizeof(names)/sizeof(names[0]); ++i)
    {
        ApplicationExpression *attempt = prepared;
        TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT, application_expression_prepare(&attempt,
            "1", names[i], NULL, &context, &error));
        TEST_ASSERT_EQUAL_PTR(prepared, attempt);
    }
    ApplicationExpression *attempt = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_expression_prepare(&attempt,
        "I+1", "I", NULL, &context, &error));
    application_expression_destroy(attempt);
    attempt = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_expression_prepare(&attempt,
        "pi+1", "pi", NULL, &context, &error));
    application_expression_destroy(attempt);
    attempt = prepared;
    context.time_limit_ms = -1;
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT, application_expression_prepare(&attempt,
        "x", "x", NULL, &context, &error));
    TEST_ASSERT_EQUAL_PTR(prepared, attempt);
}
static void test_prepare_allocation_failure_is_atomic(void)
{
    assign(1U, true, "y=1/3+i"); prepare("x");
    bool completed = false;
    for (size_t failure = 1U; failure < 2000U; ++failure)
    {
        ApplicationExpression *attempt = prepared; CalculatorError error;
        numforge_test_allocator_begin(failure);
        CalculatorStatus status = application_expression_prepare(&attempt,
            "x+y+ans+y", "x", &session, &context, &error);
        numforge_test_allocator_end();
        if (status == CALCULATOR_OK)
        {
            application_expression_destroy(attempt); completed = true; break;
        }
        TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY, status);
        TEST_ASSERT_EQUAL_PTR(prepared, attempt);
    }
    TEST_ASSERT_TRUE(completed);
}
static void test_sample_allocation_failure_preserves_previous_results(void)
{
    prepare("x*x+1/3"); argument(0U, "1+i"); argument(1U, "2"); sample(2U);
    bool completed = false;
    for (size_t failure = 1U; failure < 2000U; ++failure)
    {
        ApplicationExpressionSample *attempt = samples; CalculatorError error;
        numforge_test_allocator_begin(failure);
        CalculatorStatus status = application_expression_sample(prepared, arguments, 2U, &attempt, &error);
        numforge_test_allocator_end();
        if (status == CALCULATOR_OK)
        {
            application_expression_samples_destroy(attempt, 2U); completed = true; break;
        }
        TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY, status);
        TEST_ASSERT_EQUAL_PTR(samples, attempt);
        text(1U, "13/3");
    }
    TEST_ASSERT_TRUE(completed);
}
static void test_batch_limits_cancellation_and_recovery(void)
{
    prepare("x*x"); argument(0U, "2"); CalculatorError error;
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT, application_expression_sample(prepared,
        arguments, APPLICATION_EXPRESSION_MAX_SAMPLES + 1U, &samples, &error));
    TEST_ASSERT_NULL(samples);
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT, application_expression_sample(prepared,
        arguments, 0U, &samples, &error));
    TEST_ASSERT_EQUAL(CALCULATOR_NULL_ARGUMENT, application_expression_sample(prepared,
        NULL, 1U, &samples, &error));
    TEST_ASSERT_TRUE(numforge_budget_begin(UINT64_MAX, SIZE_MAX, SIZE_MAX));
    numforge_test_budget_expire_after(5U);
    TEST_ASSERT_EQUAL(CALCULATOR_TIME_LIMIT, application_expression_sample(prepared,
        arguments, 1U, &samples, &error));
    TEST_ASSERT_NULL(samples);
    TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_TIME, numforge_budget_failure());
    TEST_ASSERT_FALSE(numforge_budget_begin(UINT64_MAX, SIZE_MAX, SIZE_MAX));
    numforge_budget_end(); sample(1U); text(0U, "4");
}
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
static void test_points_share_cumulative_allocation_budget(void)
{
    prepare("x*x"); argument(0U, "2"); argument(1U, "2");
    numforge_alloc_stats_reset(); sample(1U);
    size_t one_point = numforge_alloc_stats_bytes();
    application_expression_samples_destroy(samples, sample_count); samples = NULL; sample_count = 0U;
    TEST_ASSERT_TRUE(numforge_budget_begin(UINT64_MAX,
        one_point + sizeof(ApplicationExpressionSample), SIZE_MAX));
    CalculatorError error;
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_TOO_LARGE, application_expression_sample(prepared,
        arguments, 2U, &samples, &error));
    TEST_ASSERT_NULL(samples);
    TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_MEMORY, numforge_budget_failure());
    numforge_budget_end(); sample(2U); text(0U, "4"); text(1U, "4");
}
#endif
static void test_maximum_batch_owns_values_independently(void)
{
    prepare("x"); argument(0U, "2/3");
    CalculatorValue *many = malloc(APPLICATION_EXPRESSION_MAX_SAMPLES * sizeof(*many));
    TEST_ASSERT_NOT_NULL(many);
    for (size_t i = 0; i < APPLICATION_EXPRESSION_MAX_SAMPLES; ++i) many[i] = arguments[0];
    CalculatorError error;
    CalculatorStatus status = application_expression_sample(prepared, many,
        APPLICATION_EXPRESSION_MAX_SAMPLES, &samples, &error);
    free(many);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, status);
    sample_count = APPLICATION_EXPRESSION_MAX_SAMPLES;
    calculator_value_destroy(&arguments[0]);
    application_expression_destroy(prepared); prepared = NULL;
    text(0U, "2/3"); text(APPLICATION_EXPRESSION_MAX_SAMPLES - 1U, "2/3");
}
static void test_graph_phases_ownership_and_faults(void)
{
    prepare("sin(3*x)+cos(x)");argument(0U,"0");
    application_expression_enable_graph_phases(prepared);
    application_expression_enable_graph_phases(prepared);
    sample(1U);
    TEST_ASSERT_EQUAL_UINT64(2U,samples[0].phase_count);
    TEST_ASSERT_FALSE(samples[0].phase_unknown);
    for (size_t i=0U;i<2U;++i) {
        bool zero=false;
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_is_zero(&zero,samples[0].phases[i]));TEST_ASSERT_TRUE(zero);
    }
    CalculatorError error;bool complete=false;
    for (size_t failure=1U;failure<10000U;++failure) {
        ApplicationExpressionSample *attempt=samples;
        numforge_test_allocator_begin(failure);
        CalculatorStatus status=application_expression_sample(prepared,arguments,1U,&attempt,&error);
        numforge_test_allocator_end();
        if (status==CALCULATOR_OK) { application_expression_samples_destroy(attempt,1U);complete=true;break; }
        TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY,status);TEST_ASSERT_EQUAL_PTR(samples,attempt);
    }
    TEST_ASSERT_TRUE(complete);
    text(0U,"1");
}
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_graph_phases_ownership_and_faults);
    RUN_TEST(test_snapshots_parameter_shadowing_and_session_lifetime);
    RUN_TEST(test_typed_complex_and_quantity_parameters);
    RUN_TEST(test_domain_gaps_do_not_abort_later_points);
    RUN_TEST(test_context_and_unused_snapshots);
    RUN_TEST(test_invalid_inputs_random_aliases_and_error_offsets);
    RUN_TEST(test_prepare_allocation_failure_is_atomic);
    RUN_TEST(test_sample_allocation_failure_preserves_previous_results);
    RUN_TEST(test_batch_limits_cancellation_and_recovery);
    RUN_TEST(test_maximum_batch_owns_values_independently);
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    RUN_TEST(test_points_share_cumulative_allocation_budget);
#endif
    return UNITY_END();
}
