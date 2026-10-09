#include "unit_web.h"
#include "numforge_alloc.h"
#include <unity.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) { numforge_test_allocator_end(); }

static NumForgeConversionOptions options(const char *target)
{
    NumForgeConversionOptions value;
    TEST_ASSERT_TRUE(numforge_web_parse_conversion_options(target, &value));
    return value;
}
static void test_query_validation(void)
{
    NumForgeConversionOptions value = options("/api/convert?to=m%2Fs&from=km%2Fh&places=3&precision=40&rounding=floor&angle=deg&notation=scientific");
    TEST_ASSERT_EQUAL_STRING("m/s", value.to);
    TEST_ASSERT_EQUAL_INT64(40, value.precision);
    TEST_ASSERT_EQUAL_INT64(3, value.context.output_scale);
    TEST_ASSERT_EQUAL(BIGDECIMAL_ROUND_FLOOR, value.context.rounding);
    TEST_ASSERT_EQUAL(CALCULATOR_ANGLE_DEGREES, value.context.angle_unit);
    const char *bad[] = {
        "/api/convert", "/api/convert?from=m", "/api/convert?from=m&to=",
        "/api/convert?from=m&to=m&from=cm", "/api/convert?from=m&to=m&action=commit",
        "/api/convert?from=m&to=m&precision=0", "/api/convert?from=m&to=m&precision=10001",
        "/api/convert?from=m&to=m&precision=+1", "/api/convert?from=m&to=m&places=-1",
        "/api/convert?from=m&to=m&rounding=bad", "/api/convert?from=m&to=m&angle=bad",
        "/api/convert?from=m&to=m&notation=bad", "/api/convert?from=m&to=m&client=bad",
        "/api/convert?from=m%00&to=m", "/api/convert?from=m%0&to=m", "/api/convert?from=m%GG&to=m",
        "/api/convert?from=m&to=m&", "/api/convert?from=m&to=m&revision=4"
    };
    for (size_t i=0; i<sizeof(bad)/sizeof(bad[0]); ++i)
        TEST_ASSERT_FALSE(numforge_web_parse_conversion_options(bad[i], &value));
    TEST_ASSERT_FALSE(numforge_web_parse_conversion_options(NULL, &value));
}
static void test_exact_and_approximate_conversion(void)
{
    const char *cases[][3] = {
        {"/api/convert?from=km&to=m", "1/3", "\"result\":\"1000/3\""},
        {"/api/convert?from=degC&to=degF", "100", "\"result\":\"212\""},
        {"/api/convert?from=MiB&to=B", "2^10", "\"result\":\"1073741824\""},
        {"/api/convert?from=deg&to=rad&precision=30&places=10", "180", "\"result\":\"3.1415926536\""},
        {"/api/convert?from=m&to=cm&precision=30&places=10", "sqrt(2)", "\"input_approximate\":true"},
        {"/api/convert?from=km%2Fh&to=m%2Fs&places=2&rounding=floor&notation=plain", "1", "\"result\":\"0.27\""},
        {"/api/convert?from=km%2Fh&to=m%2Fs&places=2&rounding=ceiling&notation=plain", "1", "\"result\":\"0.28\""}
    };
    for (size_t i=0; i<sizeof(cases)/sizeof(cases[0]); ++i)
    {
        NumForgeConversionOptions opts = options(cases[i][0]);
        char *response = NULL;
        CalculatorError error;
        const char *code = NULL;
        TEST_ASSERT_EQUAL(CALCULATOR_OK, numforge_web_convert(NULL, cases[i][1], &opts, &response, &error, &code));
        TEST_ASSERT_NOT_NULL(strstr(response, cases[i][2]));
        TEST_ASSERT_NOT_NULL(strstr(response, "\"unit\":"));
        if (i == 0) TEST_ASSERT_NOT_NULL(strstr(response, "\"approx\":\"333.3333333333\""));
        else TEST_ASSERT_NULL(strstr(response, "\"approx\":"));
        free(response);
    }
}
static void test_read_only_session_and_errors(void)
{
    CalculatorSession session = {0};
    CalculatorContext context;
    calculator_context_init(&context);
    CalculatorError error;
    char *display = NULL;
    bool reused;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_session_compute(&session, 1, true,
        "x=2/3", &context, &display, &error, &reused)); free(display);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_session_compute(&session, 2, false,
        "rand()", &context, &display, &error, &reused)); free(display);
    CalculatorSession snapshot = session;
    NumForgeConversionOptions opts = options("/api/convert?from=km&to=m");
    const char *code;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, numforge_web_convert(&session, "x+ans", &opts, &display, &error, &code));
    TEST_ASSERT_NOT_NULL(strstr(display, "\"result\":\"4000/3\"")); free(display);
    TEST_ASSERT_EQUAL_MEMORY(&snapshot, &session, sizeof(session));
    const char *inputs[] = { "x=5", "rand()", "2+rand(5)", "1/0", "unknown", "(", "" };
    for (size_t i=0; i<sizeof(inputs)/sizeof(inputs[0]); ++i)
    {
        display = NULL;
        TEST_ASSERT_NOT_EQUAL(CALCULATOR_OK, numforge_web_convert(&session, inputs[i], &opts, &display, &error, &code));
        TEST_ASSERT_NULL(display);
        TEST_ASSERT_EQUAL_MEMORY(&snapshot, &session, sizeof(session));
        if (i==0) TEST_ASSERT_EQUAL_STRING("assignment_not_allowed", code);
        if (i==1 || i==2) TEST_ASSERT_EQUAL_STRING("random_not_allowed", code);
    }
    opts = options("/api/convert?from=kg&to=m");
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT, numforge_web_convert(&session, "1", &opts, &display, &error, &code));
    TEST_ASSERT_EQUAL_STRING("incompatible_units", code);
    opts = options("/api/convert?from=bad&to=m");
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT, numforge_web_convert(&session, "1", &opts, &display, &error, &code));
    TEST_ASSERT_EQUAL_STRING("unknown_unit", code);
    calculator_session_destroy(&session);
}
static void test_failures_and_limits(void)
{
    CalculatorSession session = {0};
    NumForgeConversionOptions opts = options("/api/convert?from=deg&to=rad&snapshot=1");
    CalculatorError error;
    const char *code;
    char *response = NULL;
    numforge_test_allocator_begin(0);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, numforge_web_convert(&session, "1/3", &opts, &response, &error, &code));
    size_t count = numforge_test_allocator_call_count();
    numforge_test_allocator_end(); free(response);
    for (size_t i=1; i<=count; ++i)
    {
        CalculatorSession snapshot = session;
        numforge_test_allocator_begin(i);
        CalculatorStatus status = numforge_web_convert(&session, "1/3", &opts, &response, &error, &code);
        bool failed = numforge_test_allocator_did_fail();
        numforge_test_allocator_end();
        TEST_ASSERT_TRUE(failed);
        TEST_ASSERT_NOT_EQUAL(CALCULATOR_OK, status);
        TEST_ASSERT_NULL(response);
        TEST_ASSERT_EQUAL_MEMORY(&snapshot, &session, sizeof(session));
    }
    opts.context.time_limit_ms = 0;
    TEST_ASSERT_EQUAL(CALCULATOR_TIME_LIMIT, numforge_web_convert(&session, "2^10000", &opts, &response, &error, &code));
    TEST_ASSERT_NULL(response);
    opts.context.time_limit_ms = 5000;
    char large[CALCULATOR_MAX_INPUT_BYTES + 2]; memset(large, '1', sizeof(large)-1); large[sizeof(large)-1] = '\0';
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_TOO_LARGE, numforge_web_convert(NULL, large, &opts, &response, &error, &code));
    TEST_ASSERT_NULL(response);
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_TOO_LARGE, numforge_web_convert(NULL, "1E1000000", &opts, &response, &error, &code));
    TEST_ASSERT_NULL(response);
    TEST_ASSERT_EQUAL(CALCULATOR_NULL_ARGUMENT, numforge_web_convert(NULL, NULL, &opts, &response, &error, &code));
    TEST_ASSERT_NULL(response);
    calculator_session_destroy(&session);
}
static void test_catalog(void)
{
    char *response;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, numforge_web_unit_catalog(&response));
    TEST_ASSERT_NOT_NULL(strstr(response, "\"name_sk\":\"mikrometer\""));
    TEST_ASSERT_NOT_NULL(strstr(response, "\"id\":\"gon\""));
    TEST_ASSERT_TRUE(strlen(response)<128U*1024U);
    free(response);
    numforge_test_allocator_begin(1);
    TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY, numforge_web_unit_catalog(&response));
    numforge_test_allocator_end();
    TEST_ASSERT_NULL(response);
}
static void test_authoritative_snapshot(void)
{
    NumForgeConversionOptions opts = options("/api/convert?from=m&to=m&notation=plain&places=2&snapshot=1");
    char *response = NULL;
    CalculatorError error;
    const char *code;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, numforge_web_convert(NULL, "1/3", &opts, &response, &error, &code));
    TEST_ASSERT_NOT_NULL(strstr(response, "\"result\":\"0.33\""));
    TEST_ASSERT_NOT_NULL(strstr(response, "\"value\":{\"kind\":\"rational\",\"text\":\"1/3\",\"unit\":\"m\",\"precision\":34}"));
    free(response);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, numforge_web_convert(NULL, "sqrt(2)", &opts, &response, &error, &code));
    TEST_ASSERT_NOT_NULL(strstr(response, "\"kind\":\"decimal_approximation\""));
    TEST_ASSERT_NULL(strstr(response, "\"text\":\"1.41\""));
    free(response);
    TEST_ASSERT_FALSE(numforge_web_parse_conversion_options("/api/convert?from=m&to=m&snapshot=0", &opts));
}
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_query_validation);
    RUN_TEST(test_exact_and_approximate_conversion);
    RUN_TEST(test_read_only_session_and_errors);
    RUN_TEST(test_failures_and_limits);
    RUN_TEST(test_catalog);
    RUN_TEST(test_authoritative_snapshot);
    return UNITY_END();
}
