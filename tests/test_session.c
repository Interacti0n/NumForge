#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unity.h>

#include "session.h"
#include "formatter.h"
#include "random.h"
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
    check_result(2U, false, "ans", "1/8");
    check_result(3U, true, "1/3", "1/3");
    check_result(4U, false, "ans*3-1", "0");
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_context_set_output_scale(&context, 100));
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute("1/3", &context, &text, &error));
    check_result(5U, false, "ans", text);
    free(text);
    text = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, numforge_web_evaluate_session(&session, 6U, false,
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

static void test_approximate_answer_keeps_its_original_precision(void)
{
    CalculatorError error;
    char *stored = NULL;
    char *fresh = NULL;

    check_result(1U, true, "sqrt(2)", "1.4142135624");
    TEST_ASSERT_NULL(session.history[0].value.integer);
    TEST_ASSERT_NULL(session.history[0].value.rational);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_context_set_output_scale(&context, 100));
    TEST_ASSERT_EQUAL(CALCULATOR_OK,
        calculator_format_result(session.history[0].value.number, &context, &stored));
    check_result(2U, false, "ans", stored);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute("sqrt(2)", &context, &fresh, &error));
    TEST_ASSERT_NOT_EQUAL(0, strcmp(stored, fresh));
    free(stored);
    free(fresh);
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

static void test_random_preview_commit_and_replay(void)
{
    uint64_t expected_state = UINT64_C(0x123456789abcdef0);
    char first[37];
    char second[37];
    session.random_state = expected_state;
    session.random_initialized = true;
    calculator_random_decimal(&expected_state, first);
    calculator_random_decimal(&expected_state, second);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_context_set_output_scale(&context, -1));

    check_result(1U, false, "rand()", first);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0x123456789abcdef0), session.random_state);
    check_result(2U, false, "rand()", first);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_context_set_output_scale(&context, 10));
    {
        char *short_display = NULL;
        CalculatorError error;
        TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_session_compute(&session, 3U, false,
            "rand()", &context, &short_display, &error, NULL));
        free(short_display);
    }
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_context_set_output_scale(&context, -1));
    check_result(4U, true, "rand()", first);
    TEST_ASSERT_EQUAL_UINT64(session.preview_random_state, session.random_state);
    check_result(4U, true, "rand()", first);
    TEST_ASSERT_EQUAL_UINT(1U, session.count);
    check_result(5U, true, "rand()", second);
    TEST_ASSERT_EQUAL_UINT64(expected_state, session.random_state);
}

static void test_random_range_multiple_calls_and_failure(void)
{
    uint64_t expected_state = UINT64_C(0x137a5b9cdef01234);
    char first[37];
    char second[37];
    char expression[90];
    char *expected_sum = NULL;
    CalculatorError error;
    session.random_state = expected_state;
    session.random_initialized = true;
    calculator_random_decimal(&expected_state, first);
    calculator_random_decimal(&expected_state, second);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_context_set_output_scale(&context, -1));
    (void)snprintf(expression, sizeof(expression), "%s+%s", first, second);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute(expression, &context, &expected_sum, &error));
    check_result(1U, true, "rand()+rand()", expected_sum);
    free(expected_sum);
    TEST_ASSERT_EQUAL_UINT64(expected_state, session.random_state);

    check_error(2U, true, "rand(0)", CALCULATOR_INVALID_ARGUMENT);
    check_error(3U, true, "rand(2;2)", CALCULATOR_INVALID_ARGUMENT);
    check_error(4U, true, "rand(2;1)", CALCULATOR_INVALID_ARGUMENT);
    check_error(5U, true, "rand(1/0)", CALCULATOR_DIVISION_BY_ZERO);
    check_error(6U, true, "rand(1;2;3)", CALCULATOR_ARGUMENT_COUNT);
    TEST_ASSERT_EQUAL_UINT64(expected_state, session.random_state);
    TEST_ASSERT_EQUAL_UINT(1U, session.count);

    /* Range forms use the same first draw as the corresponding affine expression. */
    calculator_session_destroy(&session);
    session.random_state = UINT64_C(0x137a5b9cdef01234);
    session.random_initialized = true;
    (void)snprintf(expression, sizeof(expression), "2*%s", first);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute(expression, &context, &expected_sum, &error));
    check_result(1U, true, "rand(2)", expected_sum);
    free(expected_sum);
    expected_sum = NULL;

    calculator_session_destroy(&session);
    session.random_state = UINT64_C(0x137a5b9cdef01234);
    session.random_initialized = true;
    (void)snprintf(expression, sizeof(expression), "2+3*%s", first);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute(expression, &context, &expected_sum, &error));
    check_result(1U, true, "rand(2;5)", expected_sum);
    free(expected_sum);
}

static void test_notation_changes_only_display(void)
{
    bool reused = false;
    char *text = NULL;
    CalculatorError error;
    check_result(1U, true, "1E20", "1E+20");
    context.notation = CALCULATOR_NOTATION_PLAIN;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_session_compute(&session, 2U, false,
        "1E20", &context, &text, &error, &reused));
    TEST_ASSERT_TRUE(reused);
    TEST_ASSERT_EQUAL_STRING("100000000000000000000", text);
    free(text);
    text = NULL;
    context.notation = CALCULATOR_NOTATION_MATHEMATICAL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_session_compute(&session, 3U, false,
        "1E20", &context, &text, &error, &reused));
    TEST_ASSERT_TRUE(reused);
    TEST_ASSERT_EQUAL_STRING("1 × 10^20", text);
    free(text);
    TEST_ASSERT_EQUAL_UINT(1U, session.count);
    check_result(4U, true, "ans", "1 × 10^20");
    context.notation = CALCULATOR_NOTATION_AUTO;
    check_result(5U, false, "ans", "1E+20");
    TEST_ASSERT_EQUAL_UINT(2U, session.count);
}

static void test_fraction_notation_reuses_exact_answer(void)
{
    CalculatorError error;
    char *text = NULL;
    bool reused = false;

    check_result(1U, true, "1/3", "1/3");
    context.notation = CALCULATOR_NOTATION_PLAIN;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_session_compute(&session, 2U, false,
        "ans", &context, &text, &error, &reused));
    TEST_ASSERT_EQUAL_STRING("0.3333333333", text);
    free(text);
    text = NULL;
    context.notation = CALCULATOR_NOTATION_FRACTION;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_session_compute(&session, 3U, false,
        "ans", &context, &text, &error, &reused));
    TEST_ASSERT_TRUE(reused);
    TEST_ASSERT_EQUAL_STRING("1/3", text);
    free(text);
    TEST_ASSERT_EQUAL_UINT(1U, session.count);
}

static void test_plain_output_limit_preserves_session(void)
{
    check_result(1U, true, "7", "7");
    context.notation = CALCULATOR_NOTATION_PLAIN;
    check_error(2U, true, "1E100000", CALCULATOR_VALUE_TOO_LARGE);
    TEST_ASSERT_EQUAL_UINT(1U, session.count);
    context.notation = CALCULATOR_NOTATION_SCIENTIFIC;
    check_result(3U, false, "1E100000", "1E+100000");
    check_result(4U, false, "ans", "7E+0");
}


static void test_variables_preview_precision_replay_and_isolation(void)
{
    CalculatorSession other={0};
    CalculatorError error;
    char *text=NULL;
    check_result(1,false,"x = 2/3","2/3");
    TEST_ASSERT_EQUAL_UINT(0,session.variable_count);
    check_error(2,false,"x",CALCULATOR_UNDEFINED_VARIABLE);
    check_result(3,true,"x = 2/3","2/3");
    check_result(4,false,"x*3","2");
    check_result(5,true,"x=x+1","5/3");
    check_result(5,true,"x=x+1","5/3");
    check_result(6,true,"x=x+1","8/3");
    check_result(7,false,"x=9","9");
    check_result(8,false,"x*3","8");
    check_error(9,true,"x=1/0",CALCULATOR_DIVISION_BY_ZERO);
    check_result(10,false,"x*3","8");
    TEST_ASSERT_EQUAL(CALCULATOR_OK,calculator_context_set_output_scale(&context,-1));
    check_result(11,false,"x","8/3");
    check_result(12,true,"y=x","8/3");
    check_result(13,true,"x=7","7");
    check_result(14,false,"y","8/3");
    check_error(15,true,"e=2",CALCULATOR_INVALID_ARGUMENT);
    check_error(16,true,"ans=2",CALCULATOR_INVALID_ARGUMENT);
    check_error(17,true,"sin=2",CALCULATOR_INVALID_ARGUMENT);
    check_error(18,true,"a=b=2",CALCULATOR_INVALID_TOKEN);
    TEST_ASSERT_EQUAL(CALCULATOR_UNDEFINED_VARIABLE,calculator_session_compute(
        &other,1,false,"x",&context,&text,&error,NULL));
    calculator_session_destroy(&other);
    calculator_session_destroy(&session);
    check_error(1,false,"x",CALCULATOR_UNDEFINED_VARIABLE);
}

static void test_variable_confirmation_allocation_failures_are_atomic(void)
{
    size_t calls;
    char *text=NULL;
    CalculatorError error;
    check_result(1,true,"x=5","5");
    numforge_test_allocator_begin(0);
    TEST_ASSERT_EQUAL(CALCULATOR_OK,calculator_session_compute(&session,2,true,
        "x=x+1/8",&context,&text,&error,NULL));
    calls=numforge_test_allocator_call_count();numforge_test_allocator_end();free(text);
    for(size_t failure=1;failure<=calls;failure++)
    {
        calculator_session_destroy(&session);
        check_result(1,true,"x=5","5");
        numforge_test_allocator_begin(failure);
        CalculatorStatus status=calculator_session_compute(&session,2,true,"x=x+1/8",&context,&text,&error,NULL);
        TEST_ASSERT_TRUE(numforge_test_allocator_did_fail());numforge_test_allocator_end();
        TEST_ASSERT_NOT_EQUAL(CALCULATOR_OK,status);TEST_ASSERT_NULL(text);
        TEST_ASSERT_EQUAL_UINT(1,session.count);TEST_ASSERT_EQUAL_UINT(1,session.variable_count);
        check_result(3,false,"x","5");check_result(4,false,"ans","5");
    }
}

static void test_variable_limit_and_original_error_offsets(void)
{
    char input[32];
    uint64_t revision=1;
    CalculatorError error;
    char *text=NULL;
    for(size_t i=0;i<CALCULATOR_VARIABLE_CAPACITY;i++)
    {
        snprintf(input,sizeof(input),"v%c%c=1",(int)('a'+i/26U),(int)('a'+i%26U));
        check_result(revision++,true,input,"1");
    }
    check_error(revision++,true,"new=2",CALCULATOR_VALUE_TOO_LARGE);
    check_result(revision++,true,"vaa=3","3");
    TEST_ASSERT_EQUAL(CALCULATOR_UNDEFINED_VARIABLE,calculator_session_compute(&session,revision++,false,
        "vaa = missing + 1",&context,&text,&error,NULL));
    TEST_ASSERT_EQUAL_UINT(6,error.offset);
    check_result(revision++,false,"vaa","3");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_variables_preview_precision_replay_and_isolation);
    RUN_TEST(test_variable_confirmation_allocation_failures_are_atomic);
    RUN_TEST(test_variable_limit_and_original_error_offsets);
    RUN_TEST(test_preview_confirmation_and_replay);
    RUN_TEST(test_precision_is_a_snapshot_and_sessions_are_isolated);
    RUN_TEST(test_bounded_history_and_context);
    RUN_TEST(test_approximate_answer_keeps_its_original_precision);
    RUN_TEST(test_confirmation_is_atomic_on_every_allocation_failure);
    RUN_TEST(test_limits_preserve_confirmed_state);
    RUN_TEST(test_random_preview_commit_and_replay);
    RUN_TEST(test_random_range_multiple_calls_and_failure);
    RUN_TEST(test_notation_changes_only_display);
    RUN_TEST(test_fraction_notation_reuses_exact_answer);
    RUN_TEST(test_plain_output_limit_preserves_session);
    return UNITY_END();
}
