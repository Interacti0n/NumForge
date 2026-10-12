#include "graph.h"
#include "session_api.h"
#include "web_api.h"
#include "../src/internal/numforge_alloc.h"
#include <unity.h>
#include <stdlib.h>
#include <string.h>

static CalculatorContext context;
static BigRational *minimum, *maximum;
static ApplicationGraph *graph;
static ApplicationClientStore store;
static const char *client = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

void setUp(void)
{
    calculator_context_init(&context);
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_TRUE(numforge_alloc_stats_track(true));
#endif
    minimum = bigrational_create(); maximum = bigrational_create();
    TEST_ASSERT_NOT_NULL(minimum); TEST_ASSERT_NOT_NULL(maximum);
}
void tearDown(void)
{
    numforge_test_allocator_end(); numforge_budget_end();
    application_graph_destroy(graph); graph = NULL;
    bigrational_destroy(minimum); bigrational_destroy(maximum);
    minimum = NULL; maximum = NULL;
    application_client_store_destroy(&store);
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_TRUE(numforge_alloc_stats_complete());
    TEST_ASSERT_EQUAL_UINT64(0U, numforge_alloc_stats_live());
    TEST_ASSERT_TRUE(numforge_alloc_stats_track(false));
#endif
}
static void range(const char *low, const char *high)
{
    BigDecimal *value = bigdecimal_create(); TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, low));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_from_bigdecimal(minimum, value));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, high));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_from_bigdecimal(maximum, value));
    bigdecimal_destroy(value);
}
static void test_grid_is_exact_and_inclusive(void)
{
    range("0", "1"); CalculatorError error;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_graph_sample(&graph, "x*x", "x",
        minimum, maximum, 4U, NULL, &context, &error));
    const char *x[] = {"0", "1/3", "2/3", "1"};
    const char *y[] = {"0", "1/9", "4/9", "1"};
    for (size_t i = 0; i < 4U; ++i)
    {
        char *text = NULL;
        TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_value_snapshot_text(&graph->coordinates[i], &text));
        TEST_ASSERT_EQUAL_STRING(x[i], text); free(text); text = NULL;
        TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_value_snapshot_text(&graph->samples[i].value, &text));
        TEST_ASSERT_EQUAL_STRING(y[i], text); free(text);
        TEST_ASSERT_EQUAL(APPLICATION_GRAPH_REAL, application_graph_point_kind(&graph->samples[i]));
    }
}
static void test_domain_complex_and_quantity_classification(void)
{
    range("-1", "1"); CalculatorError error;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_graph_sample(&graph, "1/x", "x",
        minimum, maximum, 3U, NULL, &context, &error));
    TEST_ASSERT_EQUAL(APPLICATION_GRAPH_DOMAIN_ERROR, application_graph_point_kind(&graph->samples[1]));
    TEST_ASSERT_EQUAL(APPLICATION_GRAPH_REAL, application_graph_point_kind(&graph->samples[2]));
    application_graph_destroy(graph); graph = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_graph_sample(&graph, "sqrt(x)", "x",
        minimum, maximum, 3U, NULL, &context, &error));
    TEST_ASSERT_EQUAL(APPLICATION_GRAPH_COMPLEX, application_graph_point_kind(&graph->samples[0]));
    application_graph_destroy(graph); graph = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_graph_sample(&graph, "qty(x;\"m\")", "x",
        minimum, maximum, 3U, NULL, &context, &error));
    TEST_ASSERT_EQUAL(APPLICATION_GRAPH_QUANTITY, application_graph_point_kind(&graph->samples[0]));
}
static void test_invalid_ranges_count_and_shared_budget(void)
{
    range("1", "0"); CalculatorError error;
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT, application_graph_sample(&graph, "x", "x",
        minimum, maximum, 3U, NULL, &context, &error));
    range("0", "0");
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT, application_graph_sample(&graph, "x", "x",
        minimum, maximum, 3U, NULL, &context, &error));
    range("0", "1");
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT, application_graph_sample(&graph, "x", "x",
        minimum, maximum, 1U, NULL, &context, &error));
    TEST_ASSERT_TRUE(numforge_budget_begin(UINT64_MAX, 1U, 1U));
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_TOO_LARGE, application_graph_sample(&graph, "x", "x",
        minimum, maximum, 3U, NULL, &context, &error));
    TEST_ASSERT_NULL(graph);
    TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_MEMORY, numforge_budget_failure());
    TEST_ASSERT_FALSE(numforge_budget_begin(UINT64_MAX, SIZE_MAX, SIZE_MAX));
    numforge_budget_end();
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_graph_sample(&graph, "x", "x",
        minimum, maximum, 3U, NULL, &context, &error));
}
static void test_every_allocation_failure_is_atomic(void)
{
    range("-1", "1"); CalculatorError error;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_graph_sample(&graph, "x", "x",
        minimum, maximum, 2U, NULL, &context, &error));
    bool complete = false;
    for (size_t failure = 1U; failure < 4000U; ++failure)
    {
        ApplicationGraph *attempt = graph;
        numforge_test_allocator_begin(failure);
        CalculatorStatus status = application_graph_sample(&attempt, "x*x+1/3", "x",
            minimum, maximum, 4U, NULL, &context, &error);
        numforge_test_allocator_end();
        if (status == CALCULATOR_OK) { application_graph_destroy(attempt); complete = true; break; }
        TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY, status);
        TEST_ASSERT_EQUAL_PTR(graph, attempt);
    }
    TEST_ASSERT_TRUE(complete);
}
static void test_http_schema_projections_and_no_session_mutation(void)
{
    CalculatorSession *session = application_client_session(&store, client, true);
    CalculatorError error; char *response = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_session_compute(session, 1U, true,
        "y=1/3", &context, &response, &error, NULL)); free(response); response = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_session_compute(session, 2U, false,
        "rand()", &context, &response, &error, NULL)); free(response); response = NULL;
    CalculatorSession before = *session;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, numforge_web_session_request(&store, "POST",
        "/api/graph/sample?min=0&max=1&points=4&client=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa&at=2",
        "x+y", &response));
    TEST_ASSERT_NOT_NULL(strstr(response, "\"count\":4"));
    TEST_ASSERT_NOT_NULL(strstr(response, "\"kind\":\"real\""));
    TEST_ASSERT_EQUAL_MEMORY(&before, session, sizeof(before));
    free(response); response = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_STALE_REQUEST, numforge_web_session_request(&store, "POST",
        "/api/graph/sample?min=0&max=1&client=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa&at=1", "x", &response));
    free(response); response = NULL;
    const char *invalid[] = {
        "/api/graph/sample?min=0&max=1&points=1", "/api/graph/sample?min=0&max=1&points=1025",
        "/api/graph/sample?min=0&max=1&points=3&points=4", "/api/graph/sample?min=0&max=1&action=commit",
        "/api/graph/sample?min=0&max=1&at=0", "/api/graph/sample?min=0&max=1&angle=bad",
        "/api/graph/sample?min=1/3&max=1", "/api/graph/sample?max=1", "/api/graph/sample?min=0&max=1&unknown=2"
    };
    for (size_t i = 0; i < sizeof(invalid)/sizeof(invalid[0]); ++i)
    {
        TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT, numforge_web_session_request(&store,
            "POST", invalid[i], "x", &response)); free(response); response = NULL;
    }
    TEST_ASSERT_EQUAL_MEMORY(&before, session, sizeof(before));
}
static void test_http_allocation_failures_and_deadline_leave_no_partial_response(void)
{
    bool complete = false;
    for (size_t failure = 1U; failure < 4000U; ++failure)
    {
        char *response = NULL;
        numforge_test_allocator_begin(failure);
        CalculatorStatus status = numforge_web_session_request(&store, "POST",
            "/api/graph/sample?min=0&max=1&points=4", "x*x+1/3", &response);
        bool triggered = numforge_test_allocator_did_fail();
        numforge_test_allocator_end();
        if (status == CALCULATOR_OK)
        {
            TEST_ASSERT_NOT_NULL(response);
            TEST_ASSERT_NOT_NULL(strstr(response, "\"ok\":true"));
        }
        else
        {
            TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY, status);
            if (response != NULL)
            {
                TEST_ASSERT_NOT_NULL(strstr(response, "\"ok\":false"));
                TEST_ASSERT_NULL(strstr(response, "\"points\""));
            }
        }
        free(response);
        if (!triggered) { TEST_ASSERT_EQUAL(CALCULATOR_OK, status); complete = true; break; }
    }
    TEST_ASSERT_TRUE(complete);
    TEST_ASSERT_TRUE(numforge_budget_begin(0U, SIZE_MAX, SIZE_MAX));
    char *response = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_TIME_LIMIT, numforge_web_session_request(&store, "POST",
        "/api/graph/sample?min=0&max=1&points=4", "x", &response));
    TEST_ASSERT_NULL(response);
    TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_TIME, numforge_budget_failure());
}

static void test_adaptive_linear_curvature_and_poles(void)
{
    range("-10", "10");CalculatorError error;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_graph_adaptive(&graph,"2*t+1",NULL,minimum,maximum,NULL,&context,&error));
    TEST_ASSERT_EQUAL_STRING("t",graph->parameter);
    size_t linear_count=graph->count;
    for(size_t i=1U;i<graph->count;++i) {
        int order=0;
        TEST_ASSERT_EQUAL(BIGRATIONAL_OK,bigrational_compare(&order,graph->coordinates[i-1U].rational,graph->coordinates[i].rational));
        TEST_ASSERT_TRUE(order<0);TEST_ASSERT_TRUE(graph->connect[i]);
    }
    TEST_ASSERT_FALSE(graph->limited);
    application_graph_destroy(graph);graph=NULL;
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    /* Live-allocation bookkeeping is deliberately excluded from the production
     * timing assertion; deterministic deadline tests below retain the 5 s scope. */
    context.time_limit_ms = 60000;
#endif
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_graph_adaptive(&graph,"sin(t)",NULL,minimum,maximum,NULL,&context,&error));
    context.time_limit_ms = CALCULATOR_DEFAULT_TIME_LIMIT_MS;
    TEST_ASSERT_GREATER_THAN(linear_count,graph->count);
    TEST_ASSERT_FALSE(graph->limited);
    application_graph_destroy(graph);graph=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_graph_adaptive(&graph,"1/(x-0.1)",NULL,minimum,maximum,NULL,&context,&error));
    BigRational *pole=bigrational_create();BigDecimal *decimal=bigdecimal_create();
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_set_string(decimal,"0.1"));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK,bigrational_from_bigdecimal(pole,decimal));
    bool found=false;
    for(size_t i=1U;i<graph->count;++i) {
        int left=0,right=0;
        TEST_ASSERT_EQUAL(BIGRATIONAL_OK,bigrational_compare(&left,graph->coordinates[i-1U].rational,pole));
        TEST_ASSERT_EQUAL(BIGRATIONAL_OK,bigrational_compare(&right,graph->coordinates[i].rational,pole));
        if(left<0 && right>0){TEST_ASSERT_FALSE(graph->connect[i]);found=true;}
    }
    TEST_ASSERT_TRUE(found);TEST_ASSERT_TRUE(graph->limited);
    TEST_ASSERT_LESS_OR_EQUAL(APPLICATION_EXPRESSION_MAX_SAMPLES,graph->count);
    bigrational_destroy(pole);bigdecimal_destroy(decimal);
}
static void test_adaptive_automatic_parameter_and_read_only_session(void)
{
    range("-1","1");CalculatorError error;
    TEST_ASSERT_EQUAL(CALCULATOR_UNDEFINED_VARIABLE, application_graph_adaptive(&graph,"x+y",NULL,minimum,maximum,NULL,&context,&error));
    TEST_ASSERT_NULL(graph);
    CalculatorSession *session=application_client_session(&store,client,true);bool reused=false;char *display=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,numforge_web_evaluate_session(session,1U,true,"a=2",10,CALCULATOR_ANGLE_RADIANS,&display,&error,&reused));free(display);
    CalculatorSession before=*session;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_graph_adaptive(&graph,"a*t+ans",NULL,minimum,maximum,session,&context,&error));
    TEST_ASSERT_EQUAL_STRING("t",graph->parameter);TEST_ASSERT_EQUAL_MEMORY(&before,session,sizeof(before));
    application_graph_destroy(graph);graph=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_graph_adaptive(&graph,"a",NULL,minimum,maximum,session,&context,&error));
    TEST_ASSERT_EQUAL_STRING("x",graph->parameter);TEST_ASSERT_EQUAL_MEMORY(&before,session,sizeof(before));
}
static void test_adaptive_faults_deadline_and_http_contract(void)
{
    range("-1","1");CalculatorError error;
    numforge_test_allocator_begin(0U);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_graph_adaptive(&graph,"x*x",NULL,minimum,maximum,NULL,&context,&error));
    size_t calls=numforge_test_allocator_call_count();numforge_test_allocator_end();
    application_graph_destroy(graph);graph=NULL;
    for(size_t i=1U;i<=96U;++i) {
        size_t fault=1U+(calls-1U)*(i-1U)/95U;
        numforge_test_allocator_begin(fault);
        CalculatorStatus status=application_graph_adaptive(&graph,"x*x",NULL,minimum,maximum,NULL,&context,&error);
        numforge_test_allocator_end();TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY,status);TEST_ASSERT_NULL(graph);
    }
    TEST_ASSERT_TRUE(numforge_budget_begin(5000U,SIZE_MAX,SIZE_MAX));numforge_test_budget_expire_after(400U);
    TEST_ASSERT_EQUAL(CALCULATOR_TIME_LIMIT, application_graph_adaptive(&graph,"sin(x)",NULL,minimum,maximum,NULL,&context,&error));
    TEST_ASSERT_NULL(graph);numforge_budget_end();
    char *response=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,numforge_web_session_request(&store,"POST","/api/graph/plot?min=-1&max=1","t*t",&response));
    TEST_ASSERT_NOT_NULL(strstr(response,"\"schema_version\":2"));
    TEST_ASSERT_NOT_NULL(strstr(response,"\"parameter\":\"t\""));TEST_ASSERT_NOT_NULL(strstr(response,"\"connect\":true"));free(response);response=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,numforge_web_session_request(&store,"POST","/api/graph/plot?min=-1&max=1&points=5","x",&response));free(response);response=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_UNDEFINED_VARIABLE,numforge_web_session_request(&store,"POST","/api/graph/plot?min=-1&max=1","x+y",&response));
    TEST_ASSERT_NOT_NULL(strstr(response,"graph_parameter_ambiguous"));free(response);
}
static void test_wide_smooth_ranges_and_shared_budget_admission(void)
{
    range("-1000000000000", "1000000000000");CalculatorError error;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_graph_adaptive(&graph,"3*x+2",NULL,minimum,maximum,NULL,&context,&error));
    TEST_ASSERT_FALSE(graph->limited);
    for (size_t i=1U;i<graph->count;++i) TEST_ASSERT_TRUE(graph->connect[i]);
    application_graph_destroy(graph);graph=NULL;
    TEST_ASSERT_EQUAL_UINT64(SIZE_MAX,numforge_budget_available_bytes());
    TEST_ASSERT_EQUAL_UINT64(UINT64_MAX,numforge_budget_available_ms());
    TEST_ASSERT_TRUE(numforge_budget_begin(60000U,2U*1024U*1024U+100000U,CALCULATOR_SINGLE_ALLOCATION));
    size_t before=numforge_budget_available_bytes();
    TEST_ASSERT_EQUAL_UINT64(before,numforge_budget_available_bytes());
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_graph_adaptive(&graph,"x*x",NULL,minimum,maximum,NULL,&context,&error));
    TEST_ASSERT_TRUE(graph->limited);
    TEST_ASSERT_GREATER_THAN(0U,numforge_budget_available_bytes());
    TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_OK,numforge_budget_failure());
    TEST_ASSERT_FALSE(numforge_budget_begin(60000U,SIZE_MAX,SIZE_MAX));
    TEST_ASSERT_LESS_THAN(before,numforge_budget_available_bytes());
    numforge_budget_end();
    TEST_ASSERT_TRUE(numforge_budget_begin(0U,SIZE_MAX,SIZE_MAX));
    TEST_ASSERT_EQUAL_UINT64(0U,numforge_budget_available_ms());
    TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_OK,numforge_budget_failure());
    TEST_ASSERT_FALSE(numforge_budget_check());
    TEST_ASSERT_EQUAL_UINT64(0U,numforge_budget_available_bytes());
    TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_TIME,numforge_budget_failure());
}
static void test_adaptive_view_includes_discovered_extrema(void)
{
    range("-10","10");CalculatorError error;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_graph_adaptive(&graph,"-(x-0.3125)^2",NULL,minimum,maximum,NULL,&context,&error));
    bool zero=false;
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_is_zero(&zero,graph->view_maximum));TEST_ASSERT_TRUE(zero);
    for(size_t i=0U;i<graph->count;++i){
        if(application_graph_point_kind(&graph->samples[i])!=APPLICATION_GRAPH_REAL)continue;
        int low=0,high=0;
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_compare(&low,graph->samples[i].value.number,graph->view_minimum));
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_compare(&high,graph->samples[i].value.number,graph->view_maximum));
        TEST_ASSERT_TRUE(low>=0);TEST_ASSERT_TRUE(high<=0);
    }
}
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_adaptive_view_includes_discovered_extrema);
    RUN_TEST(test_wide_smooth_ranges_and_shared_budget_admission);
    RUN_TEST(test_adaptive_linear_curvature_and_poles);
    RUN_TEST(test_adaptive_automatic_parameter_and_read_only_session);
    RUN_TEST(test_adaptive_faults_deadline_and_http_contract);
    RUN_TEST(test_grid_is_exact_and_inclusive);
    RUN_TEST(test_domain_complex_and_quantity_classification);
    RUN_TEST(test_invalid_ranges_count_and_shared_budget);
    RUN_TEST(test_every_allocation_failure_is_atomic);
    RUN_TEST(test_http_schema_projections_and_no_session_mutation);
    RUN_TEST(test_http_allocation_failures_and_deadline_leave_no_partial_response);
    return UNITY_END();
}
