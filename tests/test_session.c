#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "session.h"
#include "web_api.h"
#include "numforge_alloc.h"

static CalculatorSession session;
static CalculatorContext context;

void setUp(void)
{
    memset(&session, 0, sizeof(session));
    calculator_context_init(&context);
}

void tearDown(void)
{
    numforge_test_allocator_end();
    calculator_session_destroy(&session);
}

static void check_result(uint64_t revision, bool commit, const char *input, const char *expected)
{
    char *text = NULL;
    CalculatorError error;

    TEST_ASSERT_EQUAL(CALCULATOR_OK,
        calculator_session_compute(&session, revision, commit, input, &context, &text, &error, NULL));
    TEST_ASSERT_EQUAL_STRING(expected, text);
    free(text);
}

static void check_error(uint64_t revision, bool commit, const char *input, CalculatorStatus expected)
{
    char *text = NULL;
    CalculatorError error;

    TEST_ASSERT_EQUAL(expected,
        calculator_session_compute(&session, revision, commit, input, &context, &text, &error, NULL));
    TEST_ASSERT_NULL(text);
    TEST_ASSERT_EQUAL(expected, error.status);
}

static void test_preview_confirmation_and_replay(void)
{
    check_error(1U, false, "ans+1", CALCULATOR_UNDEFINED_ANSWER);
    check_result(2U, false, "10", "10");
    TEST_ASSERT_EQUAL_UINT(0U, session.count);
    check_error(3U, true, "ans", CALCULATOR_UNDEFINED_ANSWER);
    check_result(4U, true, "10", "10");
    check_result(5U, false, "ans+1", "11");
    check_result(6U, false, "ans+1", "11");
    check_result(7U, true, "ans+1", "11");
    check_result(7U, true, "ans+1", "11");
    TEST_ASSERT_EQUAL_UINT(2U, session.count);
    check_result(8U, false, "ans+1", "12");
    check_result(7U, true, "ans+1", "11");
    check_error(7U, true, "ans+2", CALCULATOR_STALE_REQUEST);
    check_error(6U, true, "ans+1", CALCULATOR_STALE_REQUEST);
    check_result(9U, true, "ans+1", "12");
    check_error(7U, true, "ans+1", CALCULATOR_STALE_REQUEST);
    check_error(10U, true, "1/0", CALCULATOR_DIVISION_BY_ZERO);
    check_result(11U, false, "2ans", "24");
    TEST_ASSERT_EQUAL_UINT(3U, session.count);
    calculator_session_clear_preview(&session);
    check_result(12U, false, "ans", "12");
}

static void test_precision_is_a_snapshot_and_sessions_are_isolated(void)
{
    CalculatorSession other = {0};
    CalculatorError error;
    char *text = NULL;
    bool reused = false;

    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_context_set_output_scale(&context, 2));
    check_result(1U, true, "1/8", "0.12");
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_context_set_output_scale(&context, -1));
    check_result(2U, false, "ans", "0.125");
    check_result(3U, true, "1/3", "0.3333333333333333333333333333333333");
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_context_set_output_scale(&context, 100));
    check_result(4U, false, "ans", "0.3333333333333333333333333333333333");
    TEST_ASSERT_EQUAL(CALCULATOR_OK, numforge_web_evaluate_session(&session, 5U, false,
        "ans", 100, CALCULATOR_ANGLE_RADIANS, &text, &error, &reused));
    TEST_ASSERT_TRUE(reused);
    free(text);
    TEST_ASSERT_EQUAL(CALCULATOR_UNDEFINED_ANSWER, numforge_web_evaluate_session(&other, 1U, false,
        "ans", 10, CALCULATOR_ANGLE_RADIANS, &text, &error, NULL));
    TEST_ASSERT_NULL(text);
    calculator_session_destroy(&other);
    calculator_session_destroy(&session);
    check_error(1U, false, "ans", CALCULATOR_UNDEFINED_ANSWER);
}

static void test_bounded_history_and_context(void)
{
    context.angle_unit = CALCULATOR_ANGLE_DEGREES;
    check_result(1U, true, "sin(90)", "1");
    for (uint64_t revision = 2U; revision <= 25U; revision++)
    {
        check_result(revision, true, "ans", "1");
    }
    TEST_ASSERT_EQUAL_UINT(CALCULATOR_HISTORY_CAPACITY, session.count);
    TEST_ASSERT_EQUAL_UINT64(10U, session.history[0].revision);
    TEST_ASSERT_EQUAL(CALCULATOR_ANGLE_DEGREES, session.history[0].value.context.angle_unit);
    check_result(26U, false, "sin(ans*90)", "1");
}

static void test_confirmation_is_atomic_on_every_allocation_failure(void)
{
    size_t calls;
    char *text = NULL;
    CalculatorError error;

    check_result(1U, true, "5", "5");
    numforge_test_allocator_begin(0U);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_session_compute(&session, 2U, true,
        "ans+1/8", &context, &text, &error, NULL));
    calls = numforge_test_allocator_call_count();
    numforge_test_allocator_end();
    free(text);
    TEST_ASSERT_GREATER_THAN(0U, calls);

    for (size_t failure = 1U; failure <= calls; failure++)
    {
        calculator_session_destroy(&session);
        check_result(1U, true, "5", "5");
        numforge_test_allocator_begin(failure);
        CalculatorStatus status = calculator_session_compute(&session, 2U, true,
            "ans+1/8", &context, &text, &error, NULL);
        TEST_ASSERT_TRUE(numforge_test_allocator_did_fail());
        numforge_test_allocator_end();
        TEST_ASSERT_NOT_EQUAL(CALCULATOR_OK, status);
        TEST_ASSERT_NULL(text);
        TEST_ASSERT_EQUAL_UINT(1U, session.count);
        TEST_ASSERT_EQUAL_STRING("5", session.history[0].display);
        check_result(3U, false, "ans", "5");
    }
}

static void test_limits_preserve_confirmed_state(void)
{
    check_result(1U, true, "5", "5");
    context.time_limit_ms = 0;
    check_error(2U, true, "ans+1", CALCULATOR_TIME_LIMIT);
    context.time_limit_ms = CALCULATOR_DEFAULT_TIME_LIMIT_MS;
    check_error(3U, true, "2^1000000000", CALCULATOR_VALUE_TOO_LARGE);
    check_result(4U, false, "ans", "5");
    TEST_ASSERT_EQUAL_UINT(1U, session.count);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_preview_confirmation_and_replay);
    RUN_TEST(test_precision_is_a_snapshot_and_sessions_are_isolated);
    RUN_TEST(test_bounded_history_and_context);
    RUN_TEST(test_confirmation_is_atomic_on_every_allocation_failure);
    RUN_TEST(test_limits_preserve_confirmed_state);
    return UNITY_END();
}
