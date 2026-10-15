#include "expression.h"
#include "fast_expression.h"
#include <math.h>
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
static ApplicationFastExpression *fast;

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
    application_fast_expression_destroy(fast);fast=NULL;
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
    prepare("sin(3*x)+cos(x)+floor(x)+ceil(x)+trunc(x)+round(x;1)");argument(0U,"0");
    TEST_ASSERT_FALSE(application_expression_graph_smooth(prepared));
    application_expression_enable_graph_phases(prepared);
    application_expression_enable_graph_phases(prepared);
    sample(1U);
    TEST_ASSERT_EQUAL_UINT64(6U,samples[0].phase_count);
    TEST_ASSERT_FALSE(samples[0].phase_unknown);
    for (size_t i=0U;i<6U;++i) {
        bool zero=false;
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_is_zero(&zero,samples[0].phases[i]));TEST_ASSERT_TRUE(zero);
        if(i>=2U)TEST_ASSERT_EQUAL(-2,application_expression_graph_peak_offset(prepared,i));
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
static void test_coarse_graph_classification_rejects_poles_and_jumps(void)
{
    const char *unsafe[]={"1/x","tan(x)","floor(x)","sin(1/x)","sqrt(x)","x^(-2)","x^0.5"};
    for(size_t i=0;i<sizeof(unsafe)/sizeof(unsafe[0]);++i){
        prepare(unsafe[i]);TEST_ASSERT_FALSE(application_expression_graph_smooth(prepared));
        application_expression_destroy(prepared);prepared=NULL;
    }
    prepare("sin(3*x)*exp(-x*x/20)");TEST_ASSERT_TRUE(application_expression_graph_smooth(prepared));
}
static void compile_fast(const char *input)
{
    CalculatorError error;
    application_expression_destroy(prepared);prepared=NULL;
    application_fast_expression_destroy(fast);fast=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_expression_prepare_auto(&prepared,input,&session,&context,&error));
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_expression_fast_compile(&fast,prepared,&error));
}
static void test_dense_sum_metadata_and_failure_contract(void)
{
    ApplicationFastDenseModel model={0};
    compile_fast("sin(1000x)-sin(1000x)+cos(1100x)");
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_expression_dense_model(fast,&model));
    TEST_ASSERT_EQUAL_UINT(1U,model.count);TEST_ASSERT_TRUE(model.carriers[0].cosine);
    TEST_ASSERT_TRUE(model.carriers[0].phase==1100);TEST_ASSERT_TRUE(model.carriers[0].amplitude_offset==1);
    compile_fast("x*(sin(1000x)+cos(1100x))+2");
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_expression_dense_model(fast,&model));
    TEST_ASSERT_EQUAL_UINT(2U,model.count);TEST_ASSERT_TRUE(model.offset_value==2);
    for(size_t i=0;i<model.count;++i){TEST_ASSERT_TRUE(model.carriers[i].amplitude==1);TEST_ASSERT_TRUE(model.carriers[i].amplitude_offset==0);}
    ApplicationFastDenseModel original=model;
    TEST_ASSERT_TRUE(numforge_budget_begin(0U,SIZE_MAX,SIZE_MAX));
    TEST_ASSERT_EQUAL(CALCULATOR_TIME_LIMIT,application_fast_expression_dense_model(fast,&model));
    numforge_budget_end();TEST_ASSERT_EQUAL_MEMORY(&original,&model,sizeof(model));
    compile_fast("sin(1000x)/(x-0.1)");
    TEST_ASSERT_EQUAL(CALCULATOR_NOT_IMPLEMENTED,application_fast_expression_dense_model(fast,&model));
    TEST_ASSERT_EQUAL_MEMORY(&original,&model,sizeof(model));
}
static void test_fast_graph_values_domains_and_angle_units(void)
{
    const double x[]={-2,0,2};ApplicationFastSample output[3];
    compile_fast("sin(3x)+cos(2x)");
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_expression_sample(fast,x,3U,output));
    for(size_t i=0;i<3U;++i){TEST_ASSERT_EQUAL(APPLICATION_FAST_REAL,output[i].kind);TEST_ASSERT_TRUE(fabs(sin(3*x[i])+cos(2*x[i])-output[i].value)<1e-13);}
    compile_fast("sqrt(x)");TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_expression_sample(fast,x,3U,output));
    TEST_ASSERT_EQUAL(APPLICATION_FAST_NON_REAL,output[0].kind);TEST_ASSERT_EQUAL(APPLICATION_FAST_REAL,output[1].kind);
    compile_fast("ln(x)");TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_expression_sample(fast,x,3U,output));
    TEST_ASSERT_EQUAL(APPLICATION_FAST_NON_REAL,output[0].kind);TEST_ASSERT_EQUAL(APPLICATION_FAST_UNCERTAIN,output[1].kind);
    compile_fast("1/x");TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_expression_sample(fast,x,3U,output));
    TEST_ASSERT_EQUAL(APPLICATION_FAST_UNCERTAIN,output[1].kind);
    context.angle_unit=CALCULATOR_ANGLE_DEGREES;compile_fast("sin(x)");const double degrees[]={90};
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_expression_sample(fast,degrees,1U,output));
    TEST_ASSERT_EQUAL(APPLICATION_FAST_REAL,output[0].kind);TEST_ASSERT_TRUE(fabs(1-output[0].value)<1e-14);
}
static void test_fast_graph_uncertainty_and_unsupported_syntax(void)
{
    CalculatorError error;ApplicationFastSample output[1];const double x[]={1};
    compile_fast("(10000000000000000+x)-10000000000000000");
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_expression_sample(fast,x,1U,output));TEST_ASSERT_EQUAL(APPLICATION_FAST_UNCERTAIN,output[0].kind);
    compile_fast("sin(x)");const double large[]={1e12};
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_expression_sample(fast,large,1U,output));TEST_ASSERT_EQUAL(APPLICATION_FAST_UNCERTAIN,output[0].kind);
    compile_fast("tan(x)");const double pole[]={1.5707963267948966};
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_expression_sample(fast,pole,1U,output));TEST_ASSERT_EQUAL(APPLICATION_FAST_UNCERTAIN,output[0].kind);
    compile_fast("abs(sqrt(-1))+x");
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_expression_sample(fast,x,1U,output));TEST_ASSERT_EQUAL(APPLICATION_FAST_UNCERTAIN,output[0].kind);
    compile_fast("x*x");const double tiny_coordinate[]={1e-200};
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_expression_sample(fast,tiny_coordinate,1U,output));TEST_ASSERT_EQUAL(APPLICATION_FAST_UNCERTAIN,output[0].kind);
    ApplicationFastExpression *original=fast;
    application_expression_destroy(prepared);prepared=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_expression_prepare_auto(&prepared,"x!",NULL,&context,&error));
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,application_expression_fast_compile(&fast,prepared,&error));TEST_ASSERT_EQUAL_PTR(original,fast);
    application_expression_destroy(prepared);prepared=NULL;
    char tiny[406];memcpy(tiny,"0.",2U);memset(tiny+2,'0',400U);memcpy(tiny+402,"1*x",4U);
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_expression_prepare_auto(&prepared,tiny,NULL,&context,&error));
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,application_expression_fast_compile(&fast,prepared,&error));TEST_ASSERT_EQUAL_PTR(original,fast);
}
static void test_fast_graph_snapshot_and_arithmetic_contract(void)
{
    assign(1U,true,"y=1/3");CalculatorSession before=session;
    compile_fast("x+y+ans");TEST_ASSERT_EQUAL_MEMORY(&before,&session,sizeof(session));
    calculator_session_destroy(&session);application_expression_destroy(prepared);prepared=NULL;
    const double x[]={-2,0,2};ApplicationFastSample output[3];
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_expression_sample(fast,x,3U,output));
    for(size_t i=0;i<3U;++i){TEST_ASSERT_EQUAL(APPLICATION_FAST_REAL,output[i].kind);TEST_ASSERT_TRUE(fabs(output[i].value-x[i]-2.0/3)<1e-14);}
    compile_fast("x^2+pow(x;3)+abs(x)");
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_expression_sample(fast,x,3U,output));
    for(size_t i=0;i<3U;++i){TEST_ASSERT_EQUAL(APPLICATION_FAST_REAL,output[i].kind);TEST_ASSERT_TRUE(fabs(output[i].value-(x[i]*x[i]+x[i]*x[i]*x[i]+fabs(x[i])))<1e-14);}
    output[0].value=123;const double invalid[]={NAN};
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,application_fast_expression_sample(fast,invalid,1U,output));TEST_ASSERT_TRUE(output[0].value==123);
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,application_fast_expression_sample(fast,x,8193U,output));TEST_ASSERT_TRUE(output[0].value==123);
}
static void test_fast_graph_ownership_faults_and_shared_budget(void)
{
    CalculatorError error;compile_fast("sin(x)+1/3");
    application_expression_destroy(prepared);prepared=NULL;
    const double x[]={0};ApplicationFastSample output[1]={{123,APPLICATION_FAST_REAL}};
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_expression_sample(fast,x,1U,output));TEST_ASSERT_TRUE(fabs(1.0/3-output[0].value)<1e-14);
    output[0].value=123;numforge_test_allocator_begin(1U);
    TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY,application_fast_expression_sample(fast,x,1U,output));numforge_test_allocator_end();TEST_ASSERT_TRUE(output[0].value==123);
    TEST_ASSERT_TRUE(numforge_budget_begin(5000U,1U,CALCULATOR_SINGLE_ALLOCATION));
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_TOO_LARGE,application_fast_expression_sample(fast,x,1U,output));numforge_budget_end();TEST_ASSERT_TRUE(output[0].value==123);
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_expression_prepare_auto(&prepared,"sin(x)+1/3",NULL,&context,&error));
    ApplicationFastExpression *original=fast;numforge_test_allocator_begin(0U);
    ApplicationFastExpression *other=NULL;TEST_ASSERT_EQUAL(CALCULATOR_OK,application_expression_fast_compile(&other,prepared,&error));
    size_t calls=numforge_test_allocator_call_count();numforge_test_allocator_end();application_fast_expression_destroy(other);
    for(size_t failure=1U;failure<=calls;++failure){numforge_test_allocator_begin(failure);
        TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY,application_expression_fast_compile(&fast,prepared,&error));numforge_test_allocator_end();TEST_ASSERT_EQUAL_PTR(original,fast);}
}
static void test_forward_derivatives_and_unavailable_slopes(void)
{
    const struct {const char *expression;double x,derivative;} cases[]={
        {"x^3",2,12},{"x*x*x",-2,12},{"x^3",0,0},{"x^0",0,0},{"x^1",0,1},
        {"1/x",2,-.25},{"exp(x)",1,2.718281828459045},{"ln(x)",2,.5},
        {"sqrt(x)",4,.25},{"cbrt(x)",-8,1.0/12},{"abs(x)",-2,-1},
        {"sin(3x)+cos(2x)",.7,3*cos(2.1)-2*sin(1.4)},
        {"x^x",2,4*(log(2)+1)},{"(x-0.37)^2",.37,0},
        {"exp(-(x-0.37)^2)",.37,0}
    };
    for(size_t i=0U;i<sizeof(cases)/sizeof(cases[0]);++i){
        compile_fast(cases[i].expression);ApplicationFastProbe sample={0};
        TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_expression_probe_derivative(fast,cases[i].x,&sample));
        TEST_ASSERT_TRUE(isfinite(sample.derivative));
        TEST_ASSERT_TRUE(fabs(sample.derivative-cases[i].derivative)<1e-12*fmax(1,fabs(cases[i].derivative)));
    }
    for(size_t i=0U;i<3U;++i){
        compile_fast(i==0U?"abs(x)":i==1U?"sqrt(x)":"cbrt(x)");ApplicationFastProbe sample={0};
        TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_expression_probe_derivative(fast,0,&sample));
        TEST_ASSERT_EQUAL(APPLICATION_FAST_REAL,sample.sample.kind);TEST_ASSERT_TRUE(isnan(sample.derivative));
    }
    context.angle_unit=CALCULATOR_ANGLE_DEGREES;compile_fast("sin(x)");ApplicationFastProbe sample={0};
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_expression_probe_derivative(fast,30,&sample));
    TEST_ASSERT_TRUE(fabs(sample.derivative-cos(3.14159265358979323846/6)*3.14159265358979323846/180)<1e-14);
    sample.derivative=123;
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,application_fast_expression_probe_derivative(fast,NAN,&sample));
    TEST_ASSERT_TRUE(sample.derivative==123);
    TEST_ASSERT_TRUE(numforge_budget_begin(0U,SIZE_MAX,SIZE_MAX));
    TEST_ASSERT_EQUAL(CALCULATOR_TIME_LIMIT,application_fast_expression_probe_derivative(fast,30,&sample));
    TEST_ASSERT_TRUE(sample.derivative==123);numforge_budget_end();
}
static void test_screen_exp_tails_are_bounded_and_never_reused_as_exact_zeros(void)
{
    ApplicationFastProbe p={0};compile_fast("exp(-x*x)");
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_expression_probe_derivative(fast,100,&p));
    TEST_ASSERT_EQUAL(APPLICATION_FAST_UNCERTAIN,p.sample.kind);
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_expression_probe_screen(fast,100,1e-6,&p));
    TEST_ASSERT_EQUAL(APPLICATION_FAST_REAL,p.sample.kind);TEST_ASSERT_TRUE(p.sample.value==0);
    TEST_ASSERT_EQUAL(1U,p.exponential_count);TEST_ASSERT_TRUE(p.exponential_slopes[0]==-200);
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_expression_probe_screen(fast,100,1e-310,&p));
    TEST_ASSERT_EQUAL(APPLICATION_FAST_UNCERTAIN,p.sample.kind);
    const char *safe[]={"3-2*exp(-x*x)","exp(-x*x)/2","exp(-x*x)+exp(-2*x*x)"};
    for(size_t i=0;i<sizeof(safe)/sizeof(safe[0]);++i){compile_fast(safe[i]);
        TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_expression_probe_screen(fast,100,1e-6,&p));
        TEST_ASSERT_EQUAL(APPLICATION_FAST_REAL,p.sample.kind);TEST_ASSERT_TRUE(p.sample.value==(i==0U?3:0));}
    const char *unsafe[]={"1/exp(-x*x)","ln(exp(-x*x))","sqrt(exp(-x*x))","exp(-x*x)^0.001",
        "exp(-x*x)*10^308","exp(-x*x)*exp(-x*x)*10^308*10^308","exp(x*x)"};
    for(size_t i=0;i<sizeof(unsafe)/sizeof(unsafe[0]);++i){compile_fast(unsafe[i]);
        TEST_ASSERT_EQUAL(CALCULATOR_OK,application_fast_expression_probe_screen(fast,100,1e-6,&p));
        TEST_ASSERT_EQUAL_MESSAGE(APPLICATION_FAST_UNCERTAIN,p.sample.kind,unsafe[i]);}
    p.derivative=123;
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,application_fast_expression_probe_screen(fast,100,NAN,&p));
    TEST_ASSERT_TRUE(p.derivative==123);
    TEST_ASSERT_TRUE(numforge_budget_begin(0U,SIZE_MAX,SIZE_MAX));
    TEST_ASSERT_EQUAL(CALCULATOR_TIME_LIMIT,application_fast_expression_probe_screen(fast,100,1e-6,&p));
    numforge_budget_end();TEST_ASSERT_TRUE(p.derivative==123);
}
static void test_step_metadata_overflow_is_explicit(void)
{
    char expression[1024]="floor(x)";
    for(size_t i=1U;i<65U;++i)memcpy(expression+8U+(i-1U)*9U,"+floor(x)",10U);
    prepare(expression);argument(0U,"1/3");application_expression_enable_graph_phases(prepared);sample(1U);
    TEST_ASSERT_TRUE(samples[0].phase_unknown);TEST_ASSERT_EQUAL(64U,samples[0].phase_count);
    for(size_t i=0;i<64U;++i)TEST_ASSERT_EQUAL(-2,application_expression_graph_peak_offset(prepared,i));
}
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_step_metadata_overflow_is_explicit);
    RUN_TEST(test_screen_exp_tails_are_bounded_and_never_reused_as_exact_zeros);
    RUN_TEST(test_forward_derivatives_and_unavailable_slopes);
    RUN_TEST(test_fast_graph_values_domains_and_angle_units);
    RUN_TEST(test_dense_sum_metadata_and_failure_contract);
    RUN_TEST(test_fast_graph_snapshot_and_arithmetic_contract);
    RUN_TEST(test_fast_graph_uncertainty_and_unsupported_syntax);
    RUN_TEST(test_fast_graph_ownership_faults_and_shared_budget);
    RUN_TEST(test_coarse_graph_classification_rejects_poles_and_jumps);
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
