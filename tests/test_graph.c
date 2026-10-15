#include "graph.h"
#include "fast_graph.h"
#include "session_api.h"
#include "web_api.h"
#include "../src/internal/numforge_alloc.h"
#include <unity.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static CalculatorContext context;
static BigRational *minimum, *maximum;
static ApplicationGraph *graph;
static ApplicationFastGraph *screen_graph;
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
    application_fast_graph_destroy(screen_graph);screen_graph=NULL;
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
    TEST_ASSERT_LESS_OR_EQUAL(APPLICATION_GRAPH_MAX_ADAPTIVE_SAMPLES,graph->count);
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
static void test_peak_work_allocation_failures_are_atomic(void)
{
    range("1","2");CalculatorError error;
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    context.time_limit_ms=60000;
#endif
    numforge_test_allocator_begin(0U);
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_graph_adaptive(&graph,"sin(x)",NULL,minimum,maximum,NULL,&context,&error));
    size_t calls=numforge_test_allocator_call_count();numforge_test_allocator_end();
    bool near_one=false;
    for(size_t i=0U;i<graph->count;++i){bool one=false;BigDecimal *delta=bigdecimal_create();TEST_ASSERT_NOT_NULL(delta);
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_set_string(delta,"1"));
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_sub(delta,graph->samples[i].value.number,delta));
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_is_zero(&one,delta));near_one=near_one||one;bigdecimal_destroy(delta);
    }
    TEST_ASSERT_TRUE(near_one);application_graph_destroy(graph);graph=NULL;
    /* Later stratified failures cover optional peak scratch/evaluation/insertion,
     * including partial owned sample cleanup. Zero-live checks run in tearDown. */
    for(size_t i=0U;i<24U;++i){
        size_t fault=1U+(calls-1U)*i/23U;
        numforge_test_allocator_begin(fault);
        CalculatorStatus status=application_graph_adaptive(&graph,"sin(x)",NULL,minimum,maximum,NULL,&context,&error);
        numforge_test_allocator_end();TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY,status);TEST_ASSERT_NULL(graph);
    }
}
static void test_resolution_experiment_preserves_budget_and_atomic_output(void)
{
    range("-10","10");CalculatorError error;
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,application_graph_adaptive_limit(&graph,"x",NULL,minimum,maximum,32U,NULL,&context,&error));
    TEST_ASSERT_NULL(graph);
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,application_graph_adaptive_limit(&graph,"x",NULL,minimum,maximum,8193U,NULL,&context,&error));
    TEST_ASSERT_NULL(graph);
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_graph_adaptive_limit(&graph,"x",NULL,minimum,maximum,2048U,NULL,&context,&error));
    ApplicationGraph *original=graph;
    if(4096U*sizeof(ApplicationExpressionSample)>CALCULATOR_SINGLE_ALLOCATION){
        TEST_ASSERT_EQUAL(CALCULATOR_VALUE_TOO_LARGE,application_graph_adaptive_limit(&graph,"x",NULL,minimum,maximum,4096U,NULL,&context,&error));
        TEST_ASSERT_EQUAL_PTR(original,graph);
    }
    numforge_test_allocator_begin(1U);
    TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY,application_graph_adaptive_limit(&graph,"x",NULL,minimum,maximum,2048U,NULL,&context,&error));
    numforge_test_allocator_end();TEST_ASSERT_EQUAL_PTR(original,graph);
}
static void test_fast_screen_sampler_connections_and_atomic_failure(void)
{
    CalculatorError error;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_graph_sample(&screen_graph,"sin(33x)",NULL,-10,10,800U,440U,false,NULL,&context,&error));
    TEST_ASSERT_FALSE(screen_graph->limited);TEST_ASSERT_LESS_OR_EQUAL(APPLICATION_FAST_GRAPH_MAX_POINTS,screen_graph->count);
    for(size_t i=1;i<screen_graph->count;++i){TEST_ASSERT_TRUE(screen_graph->points[i].x>screen_graph->points[i-1U].x);TEST_ASSERT_TRUE(screen_graph->points[i].connect);}
    application_fast_graph_destroy(screen_graph);screen_graph=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_graph_sample(&screen_graph,"1/(x-0.1)",NULL,-10,10,320U,300U,false,NULL,&context,&error));
    for(size_t i=1;i<screen_graph->count;++i)if(screen_graph->points[i-1U].x<0.1&&screen_graph->points[i].x>0.1)TEST_ASSERT_FALSE(screen_graph->points[i].connect);
    ApplicationFastGraph *original=screen_graph;
    TEST_ASSERT_EQUAL(CALCULATOR_NOT_IMPLEMENTED,application_fast_graph_sample(&screen_graph,"floor(x)",NULL,-10,10,320U,300U,false,NULL,&context,&error));
    TEST_ASSERT_EQUAL_PTR(original,screen_graph);
    numforge_test_allocator_begin(1U);
    TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY,application_fast_graph_sample(&screen_graph,"x",NULL,-10,10,320U,300U,false,NULL,&context,&error));
    numforge_test_allocator_end();TEST_ASSERT_EQUAL_PTR(original,screen_graph);
    application_fast_graph_destroy(screen_graph);screen_graph=NULL;
    numforge_test_allocator_begin(0U);
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_graph_sample(&screen_graph,"sin(x)",NULL,-10,10,320U,300U,false,NULL,&context,&error));
    size_t calls=numforge_test_allocator_call_count();numforge_test_allocator_end();
    application_fast_graph_destroy(screen_graph);screen_graph=NULL;
    for(size_t failure=1;failure<=calls;++failure){numforge_test_allocator_begin(failure);
        TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY,application_fast_graph_sample(&screen_graph,"sin(x)",NULL,-10,10,320U,300U,false,NULL,&context,&error));
        numforge_test_allocator_end();TEST_ASSERT_NULL(screen_graph);
    }
    TEST_ASSERT_TRUE(numforge_budget_begin(5000U,1U,CALCULATOR_SINGLE_ALLOCATION));
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_TOO_LARGE,application_fast_graph_sample(&screen_graph,"x",NULL,-10,10,320U,300U,false,NULL,&context,&error));
    numforge_budget_end();TEST_ASSERT_NULL(screen_graph);
}
static void test_screen_camera_span_and_failure_preserve_output(void)
{
    CalculatorError error;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_graph_sample_view(&screen_graph,"sin(x)",NULL,-30,30,2048U,440U,2,false,NULL,&context,&error));
    size_t detailed=screen_graph->count;
    application_fast_graph_destroy(screen_graph);screen_graph=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_graph_sample_view(&screen_graph,"sin(x)",NULL,-30,30,2048U,440U,20,false,NULL,&context,&error));
    TEST_ASSERT_LESS_THAN(detailed,screen_graph->count);
    TEST_ASSERT_FALSE(screen_graph->limited);
    for(size_t i=1U;i<screen_graph->count;++i)TEST_ASSERT_TRUE(screen_graph->points[i].connect);
    ApplicationFastGraph *original=screen_graph;
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,application_fast_graph_sample_view(&screen_graph,"x",NULL,-10,10,800U,440U,-1,false,NULL,&context,&error));
    TEST_ASSERT_EQUAL_PTR(original,screen_graph);
    numforge_test_allocator_begin(1U);
    TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY,application_fast_graph_sample_view(&screen_graph,"x",NULL,-10,10,800U,440U,20,false,NULL,&context,&error));
    numforge_test_allocator_end();TEST_ASSERT_EQUAL_PTR(original,screen_graph);
}
static void test_screen_floor_keeps_smooth_chords_connected(void)
{
    CalculatorError error;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_graph_sample_view(&screen_graph,"x^10",NULL,-30,30,2048U,440U,20,false,NULL,&context,&error));
    TEST_ASSERT_FALSE(screen_graph->limited);
    TEST_ASSERT_LESS_OR_EQUAL(4097U,screen_graph->count);
    for(size_t i=1U;i<screen_graph->count;++i)TEST_ASSERT_TRUE(screen_graph->points[i].connect);
    application_fast_graph_destroy(screen_graph);screen_graph=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_graph_sample_view(&screen_graph,"1/(x-0.1)^2",NULL,-10,10,800U,440U,20,false,NULL,&context,&error));
    for(size_t i=1U;i<screen_graph->count;++i)
        if(screen_graph->points[i-1U].x<0.1&&screen_graph->points[i].x>0.1)TEST_ASSERT_FALSE(screen_graph->points[i].connect);
}
static void test_stationary_anchors_slopes_and_http_opt_in(void)
{
    const char *expressions[]={"x^3-3x","exp(-(x-0.37)^2)","sin(x)+x/5"};
    const double peaks[]={1,0.37,1.7721542475852274};
    CalculatorError error;
    for(size_t c=0;c<3U;++c){
        TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_graph_sample_view(&screen_graph,expressions[c],NULL,-10,10,800U,440U,20,false,NULL,&context,&error));
        double nearest=INFINITY;
        for(size_t i=0;i<screen_graph->count;++i){
            ApplicationFastGraphPoint p=screen_graph->points[i];
            if(fabs(p.x-peaks[c])<nearest){nearest=fabs(p.x-peaks[c]);}
            if(i)TEST_ASSERT_TRUE(p.connect);
        }
        TEST_ASSERT_TRUE_MESSAGE(nearest<1e-5,expressions[c]);
        application_fast_graph_destroy(screen_graph);screen_graph=NULL;
    }
    char *response=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,numforge_web_session_request(&store,"POST",
        "/api/graph/plot?mode=fast&min=-1&max=1&ymin=-10&ymax=10&interpolation=hermite","x^3",&response));
    TEST_ASSERT_NOT_NULL(strstr(response,"\"slopes\":["));free(response);response=NULL;
    const char *invalid[]={
        "/api/graph/plot?min=-1&max=1&interpolation=hermite",
        "/api/graph/plot?mode=fast&min=-1&max=1&interpolation=hermite",
        "/api/graph/plot?mode=fast&min=-1&max=1&ymin=-1&ymax=1&interpolation=bad",
        "/api/graph/plot?mode=fast&min=-1&max=1&ymin=-1&ymax=1&interpolation=hermite&interpolation=linear"};
    for(size_t i=0;i<sizeof(invalid)/sizeof(*invalid);++i){
        TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,numforge_web_session_request(&store,"POST",invalid[i],"x",&response));
        free(response);response=NULL;
    }
    TEST_ASSERT_TRUE(numforge_budget_begin(0U,SIZE_MAX,SIZE_MAX));
    TEST_ASSERT_EQUAL(CALCULATOR_TIME_LIMIT,application_fast_graph_sample_view(&screen_graph,"x^3-3x",NULL,-10,10,800U,440U,20,false,NULL,&context,&error));
    TEST_ASSERT_NULL(screen_graph);numforge_budget_end();
}
static void test_dense_carrier_bands_values_and_domain_exclusions(void)
{
    CalculatorError error;
    const char *cases[]={"sin(1000x)","2-3*cos(-1000x+\xCF\x80/3)","x*sin(1000x)+2","sin(1000x)+x"};
    for(size_t c=0;c<4U;++c){
        TEST_ASSERT_EQUAL_MESSAGE(CALCULATOR_OK,application_fast_graph_sample_view_dense(&screen_graph,cases[c],NULL,-10,10,800U,440U,20,false,true,NULL,&context,&error),cases[c]);
        TEST_ASSERT_FALSE(screen_graph->limited);TEST_ASSERT_EQUAL_UINT(800U,screen_graph->band_count);TEST_ASSERT_EQUAL_UINT(33U,screen_graph->count);
        for(size_t i=0;i<screen_graph->band_count;++i){ApplicationFastGraphBand band=screen_graph->bands[i];
            TEST_ASSERT_TRUE(band.high>band.low);if(i)TEST_ASSERT_TRUE(screen_graph->bands[i-1U].high==band.low);
            double amplitude=c==1U?3:c==2U?fmax(fabs(band.low),fabs(band.high)):1;
            double low=c==1U||c==2U?2:c==3U?band.low:0,high=c==3U?band.high:low;
            TEST_ASSERT_TRUE(fabs((low-amplitude)-band.minimum)<=1e-12);TEST_ASSERT_TRUE(fabs((high+amplitude)-band.maximum)<=1e-12);
            for(unsigned j=0;j<=8U;++j){double x=band.low+(band.high-band.low)*(double)j/8;
                double y=c==0U?sin(1000*x):c==1U?2-3*cos(-1000*x+3.14159265358979323846/3):c==2U?x*sin(1000*x)+2:sin(1000*x)+x;
                TEST_ASSERT_TRUE(y>=band.minimum-1e-12&&y<=band.maximum+1e-12);
            }
        }
        for(size_t i=0;i<screen_graph->count;++i)TEST_ASSERT_FALSE(screen_graph->points[i].connect);
        application_fast_graph_destroy(screen_graph);screen_graph=NULL;
    }
    const char *excluded[]={"tan(1000x)","sin(1000x)/(x-0.1)","sin(1000x)-sin(1000x)","sqrt(x)*sin(1000x)","sin(1000x*x)"};
    for(size_t i=0;i<5U;++i){
        CalculatorStatus status=application_fast_graph_sample_view_dense(&screen_graph,excluded[i],NULL,-1,1,320U,300U,20,false,true,NULL,&context,&error);
        TEST_ASSERT_TRUE_MESSAGE(status==CALCULATOR_OK||status==CALCULATOR_NOT_IMPLEMENTED,excluded[i]);
        if(status==CALCULATOR_OK)TEST_ASSERT_EQUAL_UINT(0U,screen_graph->band_count);else TEST_ASSERT_NULL(screen_graph);
        application_fast_graph_destroy(screen_graph);screen_graph=NULL;
    }
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_graph_sample_view_dense(&screen_graph,"sin(1000x)",NULL,-.01,.01,800U,440U,20,false,true,NULL,&context,&error));
    TEST_ASSERT_EQUAL_UINT(0U,screen_graph->band_count);TEST_ASSERT_FALSE(screen_graph->limited);
    for(size_t i=1;i<screen_graph->count;++i)TEST_ASSERT_TRUE(screen_graph->points[i].connect);
}
static void test_dense_bands_allocation_deadline_and_http_contract(void)
{
    CalculatorError error;
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,application_fast_graph_sample_view_dense(&screen_graph,"sin(1000x)",NULL,-10,10,800U,440U,0,false,true,NULL,&context,&error));
    TEST_ASSERT_NULL(screen_graph);
    numforge_test_allocator_begin(0U);
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_graph_sample_view_dense(&screen_graph,"sin(1000x)",NULL,-10,10,800U,440U,20,false,true,NULL,&context,&error));
    size_t calls=numforge_test_allocator_call_count();numforge_test_allocator_end();ApplicationFastGraph *original=screen_graph;
    for(size_t failure=1;failure<=calls;++failure){numforge_test_allocator_begin(failure);
        TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY,application_fast_graph_sample_view_dense(&screen_graph,"sin(1000x)",NULL,-10,10,800U,440U,20,false,true,NULL,&context,&error));
        numforge_test_allocator_end();TEST_ASSERT_EQUAL_PTR(original,screen_graph);
    }
    TEST_ASSERT_TRUE(numforge_budget_begin(0U,SIZE_MAX,SIZE_MAX));
    TEST_ASSERT_EQUAL(CALCULATOR_TIME_LIMIT,application_fast_graph_sample_view_dense(&screen_graph,"sin(1000x)",NULL,-10,10,800U,440U,20,false,true,NULL,&context,&error));
    numforge_budget_end();TEST_ASSERT_EQUAL_PTR(original,screen_graph);
    application_fast_graph_destroy(screen_graph);screen_graph=NULL;
    /* A finished envelope needs no unused 300ms midpoint-refinement reserve. */
    TEST_ASSERT_TRUE(numforge_budget_begin(250U,SIZE_MAX,SIZE_MAX));
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_graph_sample_view_dense(&screen_graph,"sin(1000x)",NULL,-10,10,800U,440U,20,false,true,NULL,&context,&error));
    TEST_ASSERT_FALSE(screen_graph->limited);TEST_ASSERT_EQUAL_UINT(800U,screen_graph->band_count);numforge_budget_end();
    char *response=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,numforge_web_session_request(&store,"POST","/api/graph/plot?mode=fast&min=-10&max=10&ymin=-10&ymax=10&dense=band","sin(1000x)",&response));
    TEST_ASSERT_NOT_NULL(strstr(response,"\"dense_representation\":\"pixel-envelope\""));TEST_ASSERT_NOT_NULL(strstr(response,"\"bands\":["));free(response);response=NULL;
    const char *invalid[]={"/api/graph/plot?min=-10&max=10&dense=band","/api/graph/plot?mode=fast&min=-10&max=10&dense=band",
        "/api/graph/plot?mode=fast&min=-10&max=10&ymin=-10&ymax=10&dense=curve","/api/graph/plot?mode=fast&min=-10&max=10&ymin=-10&ymax=10&dense=band&dense=band"};
    for(size_t i=0;i<4U;++i){TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,numforge_web_session_request(&store,"POST",invalid[i],"sin(1000x)",&response));free(response);response=NULL;}
}
static void test_camera_nested_log_domain_and_pole(void)
{
    CalculatorError error;const double pole=exp(2);
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_graph_sample_camera(&screen_graph,"1/ln(ln(sqrt(x)))^2",NULL,-10,10,800U,440U,-10,10,false,false,NULL,&context,&error));
    TEST_ASSERT_TRUE(screen_graph->screen_clipped);TEST_ASSERT_LESS_THAN(512U,screen_graph->count);
    size_t visible_edges=0;
    for(size_t i=0;i<screen_graph->count;++i){ApplicationFastGraphPoint p=screen_graph->points[i];
        if(p.kind==APPLICATION_GRAPH_REAL){TEST_ASSERT_TRUE(p.x>1);double reference=1/pow(log(log(sqrt(p.x))),2);
            TEST_ASSERT_TRUE(fabs(p.y-reference)<=fabs(reference)*1e-8+1e-12);
        }
        if(i&&p.connect){ApplicationFastGraphPoint a=screen_graph->points[i-1U];
            TEST_ASSERT_TRUE(a.x>1);TEST_ASSERT_FALSE(a.x<pole&&p.x>pole);
            double x=a.x+(p.x-a.x)/2,y=1/pow(log(log(sqrt(x))),2);
            if(y<=10){++visible_edges;TEST_ASSERT_TRUE(fabs(y-(a.y+p.y)/2)*440/20<2);}
        }
    }
    TEST_ASSERT_GREATER_THAN(8U,visible_edges);
    application_fast_graph_destroy(screen_graph);screen_graph=NULL;
    // A complex intermediate may become real. It needs the precision sampler,
    // even when fewer than half of the seeds follow that complex path.
    TEST_ASSERT_EQUAL(CALCULATOR_NOT_IMPLEMENTED,application_fast_graph_sample_camera(&screen_graph,"abs(sqrt(x))",NULL,-10,10,800U,440U,-10,10,false,false,NULL,&context,&error));
    TEST_ASSERT_NULL(screen_graph);
    char *response=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,numforge_web_session_request(&store,"POST","/api/graph/plot?mode=fast&min=-10&max=10&ymin=-10&ymax=10","1/ln(ln(sqrt(x)))^2",&response));
    TEST_ASSERT_NOT_NULL(strstr(response,"\"evaluation\":\"double\""));TEST_ASSERT_NOT_NULL(strstr(response,"\"screen_clipped\":true"));free(response);
}
static void test_camera_clipping_failure_and_deadline_are_atomic(void)
{
    CalculatorError error;
    numforge_test_allocator_begin(0U);
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_graph_sample_camera(&screen_graph,"1/(x-0.1)^2",NULL,-10,10,800U,440U,-10,10,false,false,NULL,&context,&error));
    size_t calls=numforge_test_allocator_call_count();numforge_test_allocator_end();ApplicationFastGraph *original=screen_graph;
    TEST_ASSERT_TRUE(screen_graph->screen_clipped);
    for(size_t failure=1;failure<=calls;++failure){numforge_test_allocator_begin(failure);
        TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY,application_fast_graph_sample_camera(&screen_graph,"1/(x-0.1)^2",NULL,-10,10,800U,440U,-10,10,false,false,NULL,&context,&error));
        numforge_test_allocator_end();TEST_ASSERT_EQUAL_PTR(original,screen_graph);
    }
    TEST_ASSERT_TRUE(numforge_budget_begin(0U,SIZE_MAX,SIZE_MAX));
    TEST_ASSERT_EQUAL(CALCULATOR_TIME_LIMIT,application_fast_graph_sample_camera(&screen_graph,"x*x",NULL,-10,10,800U,440U,-10,10,false,false,NULL,&context,&error));
    numforge_budget_end();TEST_ASSERT_EQUAL_PTR(original,screen_graph);
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,application_fast_graph_sample_camera(&screen_graph,"x",NULL,-10,10,800U,440U,10,-10,false,false,NULL,&context,&error));
    TEST_ASSERT_EQUAL_PTR(original,screen_graph);
}
static void test_precision_camera_clipping_and_atomic_failures(void)
{
    range("-10","10");CalculatorError error;
    BigDecimal *low=bigdecimal_create(), *high=bigdecimal_create();
    TEST_ASSERT_NOT_NULL(low);TEST_ASSERT_NOT_NULL(high);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_set_string(low,"-10"));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_set_string(high,"10"));
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    context.time_limit_ms=60000;
#endif
    numforge_test_allocator_begin(0U);
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_graph_adaptive_camera(&graph,"sinh(10x)",NULL,minimum,maximum,low,high,800U,440U,NULL,&context,&error));
    size_t calls=numforge_test_allocator_call_count();numforge_test_allocator_end();
    TEST_ASSERT_TRUE(graph->screen_clipped);TEST_ASSERT_FALSE(graph->limited);TEST_ASSERT_LESS_THAN(1024U,graph->count);
    size_t visible=0,gaps=0;
    for(size_t i=1;i<graph->count;++i){
        if(graph->connect[i])++visible;else ++gaps;
    }
    TEST_ASSERT_GREATER_THAN(16U,visible);TEST_ASSERT_GREATER_THAN(16U,gaps);
    ApplicationGraph *original=graph;
    for(size_t i=0;i<20U;++i){
        numforge_test_allocator_begin(1U+(calls-1U)*i/19U);
        TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY,application_graph_adaptive_camera(&graph,"sinh(10x)",NULL,minimum,maximum,low,high,800U,440U,NULL,&context,&error));
        numforge_test_allocator_end();TEST_ASSERT_EQUAL_PTR(original,graph);
    }
    TEST_ASSERT_TRUE(numforge_budget_begin(0U,SIZE_MAX,SIZE_MAX));
    TEST_ASSERT_EQUAL(CALCULATOR_TIME_LIMIT,application_graph_adaptive_camera(&graph,"sinh(10x)",NULL,minimum,maximum,low,high,800U,440U,NULL,&context,&error));
    numforge_budget_end();TEST_ASSERT_EQUAL_PTR(original,graph);
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,application_graph_adaptive_camera(&graph,"x",NULL,minimum,maximum,high,low,800U,440U,NULL,&context,&error));
    TEST_ASSERT_EQUAL_PTR(original,graph);
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,application_graph_adaptive_camera(&graph,"x",NULL,minimum,maximum,low,high,0U,440U,NULL,&context,&error));
    TEST_ASSERT_EQUAL(CALCULATOR_NULL_ARGUMENT,application_graph_adaptive_camera(&graph,"x",NULL,minimum,maximum,NULL,high,800U,440U,NULL,&context,&error));
    TEST_ASSERT_EQUAL_PTR(original,graph);
    char *response=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,numforge_web_session_request(&store,"POST","/api/graph/plot?mode=fast&min=-10&max=10&ymin=-10&ymax=10&precision=35&quality=coarse","sinh(10x)",&response));
    TEST_ASSERT_NOT_NULL(strstr(response,"\"schema_version\":2"));TEST_ASSERT_NOT_NULL(strstr(response,"\"screen_clipped\":true"));
    TEST_ASSERT_NOT_NULL(strstr(response,"\"quality\":\"refined\""));free(response);response=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,numforge_web_session_request(&store,"POST","/api/graph/plot?min=-10&max=10","sinh(10x)",&response));
    TEST_ASSERT_NULL(strstr(response,"screen_clipped"));TEST_ASSERT_NULL(strstr(response,"\"width\""));free(response);
    bigdecimal_destroy(low);bigdecimal_destroy(high);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_precision_camera_clipping_and_atomic_failures);
    RUN_TEST(test_camera_nested_log_domain_and_pole);
    RUN_TEST(test_camera_clipping_failure_and_deadline_are_atomic);
    RUN_TEST(test_dense_carrier_bands_values_and_domain_exclusions);
    RUN_TEST(test_dense_bands_allocation_deadline_and_http_contract);
    RUN_TEST(test_stationary_anchors_slopes_and_http_opt_in);
    RUN_TEST(test_screen_floor_keeps_smooth_chords_connected);
    RUN_TEST(test_screen_camera_span_and_failure_preserve_output);
    RUN_TEST(test_fast_screen_sampler_connections_and_atomic_failure);
    RUN_TEST(test_resolution_experiment_preserves_budget_and_atomic_output);
    RUN_TEST(test_peak_work_allocation_failures_are_atomic);
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
