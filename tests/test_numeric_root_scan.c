#include "src/application/equations/numeric_root.h"
#include "apps/web/http/session_api.h"
#include "src/runtime/numforge_alloc.h"
#include <unity.h>
#include <stdlib.h>
#include <string.h>

static CalculatorContext context;
static CalculatorError error;
static BigRational *low, *high;
static ApplicationRootScan *result;
/* The full session pool is larger than Windows' default thread stack. */
static ApplicationClientStore store;
void setUp(void)
{
    calculator_context_init(&context);
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_TRUE(numforge_alloc_stats_track(true));
#endif
    low = bigrational_create(); high = bigrational_create();
    TEST_ASSERT_NOT_NULL(low); TEST_ASSERT_NOT_NULL(high);
}
void tearDown(void)
{
    numforge_test_allocator_end(); numforge_budget_end();
    application_numeric_roots_destroy(result); result = NULL;
    application_client_store_destroy(&store);
    bigrational_destroy(low); bigrational_destroy(high);
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_TRUE(numforge_alloc_stats_complete()); TEST_ASSERT_EQUAL_UINT64(0U, numforge_alloc_stats_live());
    TEST_ASSERT_TRUE(numforge_alloc_stats_track(false));
#endif
}
static void bound(BigRational *value, const char *text)
{
    BigDecimal *decimal = bigdecimal_create(); TEST_ASSERT_NOT_NULL(decimal);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(decimal, text));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_from_bigdecimal(value, decimal)); bigdecimal_destroy(decimal);
}
static void scan(const char *input, const char *from, const char *to, size_t count)
{
    application_numeric_roots_destroy(result); result = NULL; bound(low, from); bound(high, to);
    TEST_ASSERT_EQUAL_MESSAGE(CALCULATOR_OK, application_numeric_roots_scan(&result, input, low, high, &context, &error), input);
    TEST_ASSERT_NOT_NULL(result); TEST_ASSERT_EQUAL_UINT64(count, result->count);
    TEST_ASSERT_LESS_OR_EQUAL_UINT64(APPLICATION_ROOT_SCAN_MAX_EVALUATIONS, result->evaluations);
    TEST_ASSERT_LESS_OR_EQUAL_UINT64(APPLICATION_ROOT_SCAN_SEGMENTS, result->scanned_segments);
    for (size_t i = 0U; i < count; ++i) {
        int order = 0;
        TEST_ASSERT_EQUAL(APPLICATION_ROOT_FOUND, result->roots[i]->kind);
        TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_compare(&order, low, result->roots[i]->x)); TEST_ASSERT_TRUE(order <= 0);
        TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_compare(&order, high, result->roots[i]->x)); TEST_ASSERT_TRUE(order >= 0);
        if (i) { TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_compare(&order, result->roots[i-1U]->x, result->roots[i]->x)); TEST_ASSERT_TRUE(order < 0); }
    }
}
static void near(size_t index, const char *reference)
{
    BigDecimal *actual = bigdecimal_create(), *expected = bigdecimal_create(), *tolerance = bigdecimal_create();
    TEST_ASSERT_NOT_NULL(actual); TEST_ASSERT_NOT_NULL(expected); TEST_ASSERT_NOT_NULL(tolerance); int order = 1;
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_to_bigdecimal(actual, result->roots[index]->x, 140, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(expected, reference));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_sub(actual, actual, expected));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_abs(actual, actual));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(tolerance, "1E-12"));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_compare(&order, actual, tolerance)); TEST_ASSERT_TRUE(order <= 0);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_compare(&order, result->roots[index]->residual, tolerance)); TEST_ASSERT_TRUE(order <= 0);
    bigdecimal_destroy(actual); bigdecimal_destroy(expected); bigdecimal_destroy(tolerance);
}
static void test_ordered_roots_and_shared_endpoint_uniqueness(void)
{
    scan("x*(x-2)*(x+3)=0", "-4", "4", 3U); near(0U,"-3"); near(1U,"0"); near(2U,"2");
    TEST_ASSERT_EQUAL(APPLICATION_SCAN_FINISHED, result->stop); TEST_ASSERT_EQUAL_UINT64(128U, result->scanned_segments);
    scan("x*(x-1)=0", "0", "1", 2U); near(0U,"0"); near(1U,"1");
    scan("(x-0.125)^2=0", "0", "1", 1U); near(0U,"0.125");
}
static void test_transcendental_references_at_precision_extremes(void)
{
    const int digits[] = {16,34,100};
    const char *references[] = {"-9.424777960769379715387930149838508653", "-6.283185307179586476925286766559005768",
        "-3.141592653589793238462643383279502884", "0", "3.141592653589793238462643383279502884",
        "6.283185307179586476925286766559005768", "9.424777960769379715387930149838508653"};
    for (size_t p = 0U; p < 3U; ++p) {
        context.division_scale = digits[p]; scan("sin(x)=0", "-10", "10", 7U);
        for (size_t i = 0U; i < 7U; ++i) near(i,references[i]);
        scan("cos(x)=x", "-2", "2", 1U); near(0U,"0.739085133215160641655312087673873404");
    }
}
static void test_domain_gaps_and_rejected_brackets_do_not_become_roots(void)
{
    scan("1/x=0", "-1", "1", 0U); TEST_ASSERT_EQUAL_UINT64(2U, result->domain_gaps);
    scan("ln(x)=0", "-2", "2", 1U); near(0U,"1"); TEST_ASSERT_TRUE(result->domain_gaps > 0U);
    scan("x-0.1234+1E-30*(x-0.1234)/abs(x-0.1234)=0", "0", "1", 0U);
    TEST_ASSERT_EQUAL_UINT64(1U, result->rejected_brackets);
    scan("1E-100/(x-0.1234)=0", "0", "1", 0U); TEST_ASSERT_EQUAL_UINT64(1U, result->rejected_brackets);
}
static void test_grid_is_explicitly_incomplete_and_limits_are_bounded(void)
{
    scan("(x-0.1234)^2=0", "0", "1", 1U); near(0U,"0.1234"); TEST_ASSERT_EQUAL(APPLICATION_SCAN_FINISHED, result->stop);
    TEST_ASSERT_EQUAL_UINT64(1U,result->candidate_roots); TEST_ASSERT_TRUE(result->roots[0]->exact_zero);
    scan("(x-0.01)*(x-0.02)=0", "0", "128", 2U); near(0U,"0.01"); near(1U,"0.02");
    scan("x=x", "0", "1", 32U); TEST_ASSERT_EQUAL(APPLICATION_SCAN_ROOT_LIMIT, result->stop); TEST_ASSERT_LESS_THAN_UINT64(128U, result->scanned_segments);
    scan("2*mod(floor(x);2)-1=0", "0", "128", 0U);
    TEST_ASSERT_EQUAL(APPLICATION_SCAN_EVALUATION_LIMIT, result->stop); TEST_ASSERT_TRUE(result->rejected_brackets > 0U);
}
static void test_quadratic_contacts_keep_exact_rational_ordinates(void)
{
    const int digits[] = {16,34,100};
    for (size_t p = 0U; p < 3U; ++p) {
        context.division_scale=digits[p];
        scan("9*x^2-6*x+1=0","0","1",1U); near(0U,"0.333333333333333333333333333333333333");
        TEST_ASSERT_TRUE(result->roots[0]->exact_zero);
        scan("(x-1/3)^2=0","0","1",1U); near(0U,"0.333333333333333333333333333333333333");
        scan("-1E-100*(x-0.1234)^2=0","0","1",1U); near(0U,"0.1234");
        scan("(x-0.1234)^2+1E-100=0","0","1",0U);
        scan("(x-0.1234)^2-1E-20=0","0","1",2U); near(0U,"0.1233999999"); near(1U,"0.1234000001");
    }
    scan("(cos(x)-x)^2=0","0","1",0U); // Rounded minima are not root proofs.
    scan("1/((x-0.1234)^2)=0","0","1",0U);
    scan("(x-1/3)^2=0/(x-1/3)","0","1",0U);
    TEST_ASSERT_EQUAL_UINT64(1U,result->candidate_gaps);
    scan("(x-1E-101)^2=0","0","1E-100",1U);
    BigRational *expected = bigrational_create(); TEST_ASSERT_NOT_NULL(expected); bound(expected,"1E-101");
    int order=1; TEST_ASSERT_EQUAL(BIGRATIONAL_OK,bigrational_compare(&order,result->roots[0]->x,expected)); TEST_ASSERT_EQUAL_INT(0,order);
    bigrational_destroy(expected);
}
static void test_candidate_coordinates_preserve_large_common_offsets(void)
{
    BigRational *one=bigrational_create(), *three=bigrational_create(), *expected=bigrational_create();
    TEST_ASSERT_NOT_NULL(one); TEST_ASSERT_NOT_NULL(three); TEST_ASSERT_NOT_NULL(expected);
    bound(low,"1E100"); bound(one,"1"); bound(three,"3");
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK,bigrational_add(high,low,one));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK,bigrational_div(expected,one,three)); TEST_ASSERT_EQUAL(BIGRATIONAL_OK,bigrational_add(expected,expected,low));
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_numeric_roots_scan(&result,"(x-1E100-1/3)^2=0",low,high,&context,&error));
    TEST_ASSERT_EQUAL_UINT64(1U,result->count); TEST_ASSERT_TRUE(result->roots[0]->exact_zero);
    int order=1; TEST_ASSERT_EQUAL(BIGRATIONAL_OK,bigrational_compare(&order,result->roots[0]->x,expected)); TEST_ASSERT_EQUAL_INT(0,order);
    bigrational_destroy(one); bigrational_destroy(three); bigrational_destroy(expected);
}
static void test_failures_preserve_outputs_and_inherit_the_outer_budget(void)
{
    scan("x=0", "-1", "1", 1U); ApplicationRootScan *original = result;
    TEST_ASSERT_EQUAL(CALCULATOR_SYNTAX_ERROR, application_numeric_roots_scan(&result,"x=0=1",low,high,&context,&error)); TEST_ASSERT_EQUAL_PTR(original,result);
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT, application_numeric_roots_scan(&result,"rand()=x",low,high,&context,&error)); TEST_ASSERT_EQUAL_PTR(original,result);
    TEST_ASSERT_EQUAL(CALCULATOR_UNDEFINED_VARIABLE, application_numeric_roots_scan(&result,"y=x",low,high,&context,&error)); TEST_ASSERT_EQUAL_PTR(original,result);
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT, application_numeric_roots_scan(&result,"x=0",high,low,&context,&error)); TEST_ASSERT_EQUAL_PTR(original,result);
    TEST_ASSERT_TRUE(numforge_budget_begin(5000U,1U,1U));
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_TOO_LARGE, application_numeric_roots_scan(&result,"x=0",low,high,&context,&error)); TEST_ASSERT_EQUAL_PTR(original,result); numforge_budget_end();
    bound(low,"-10"); bound(high,"10"); TEST_ASSERT_TRUE(numforge_budget_begin(5000U,2U*1024U*1024U,512U*1024U));
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_TOO_LARGE, application_numeric_roots_scan(&result,"sin(x)=0",low,high,&context,&error)); TEST_ASSERT_EQUAL_PTR(original,result); numforge_budget_end();
    bound(low,"-1"); bound(high,"1");
    ApplicationRootScan *attempt = NULL; numforge_test_allocator_begin(0U);
    TEST_ASSERT_EQUAL(CALCULATOR_OK,application_numeric_roots_scan(&attempt,"cos(x)=x",low,high,&context,&error));
    size_t calls = numforge_test_allocator_call_count(); numforge_test_allocator_end(); application_numeric_roots_destroy(attempt);
    for (size_t i = 1U; i <= 160U; ++i) {
        size_t failure = i <= 80U ? i : 1U+(calls-1U)*(i-80U)/80U;
        numforge_test_allocator_begin(failure);
        TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY,application_numeric_roots_scan(&result,"cos(x)=x",low,high,&context,&error));
        numforge_test_allocator_end(); TEST_ASSERT_EQUAL_PTR(original,result);
    }
    for (size_t i = 1U; i <= 40U; ++i) {
        numforge_test_budget_expire_after(i*100U);
        TEST_ASSERT_EQUAL(CALCULATOR_TIME_LIMIT,application_numeric_roots_scan(&result,"cos(x)=x",low,high,&context,&error));
        numforge_test_allocator_end(); TEST_ASSERT_EQUAL_PTR(original,result);
    }
}
static void test_http_scan_contract_and_full_coordinates(void)
{
    char *response = NULL;
    const char *target = "/api/equations/solve?mode=numeric_scan&min=-2&max=2&precision=34";
    TEST_ASSERT_EQUAL(CALCULATOR_OK,numforge_web_session_request(&store,"POST",target,"x^2=1",&response));
    TEST_ASSERT_NOT_NULL(strstr(response,"\"kind\":\"numeric_scan\"")); TEST_ASSERT_NOT_NULL(strstr(response,"\"complete\":false"));
    TEST_ASSERT_NOT_NULL(strstr(response,"\"count\":2")); TEST_ASSERT_NOT_NULL(strstr(response,"\"outcome\":\"scanned\""));
    TEST_ASSERT_NOT_NULL(strstr(response,"\"x\":\"-1\"")); TEST_ASSERT_NOT_NULL(strstr(response,"\"x\":\"1\""));
    free(response); response = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,numforge_web_session_request(&store,"POST",target,"1/x=0",&response));
    TEST_ASSERT_NOT_NULL(strstr(response,"\"count\":0")); TEST_ASSERT_NOT_NULL(strstr(response,"\"domain_gaps\":2")); free(response); response = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,numforge_web_session_request(&store,"POST","/api/equations/solve?mode=numeric_scan&min=2&max=-2","x=0",&response));
    TEST_ASSERT_NOT_NULL(strstr(response,"numeric_options_error")); free(response); application_client_store_destroy(&store);
    response=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,numforge_web_session_request(&store,"POST",target,"(x-1/3)^2=0",&response));
    TEST_ASSERT_NOT_NULL(strstr(response,"\"x\":\"1/3\"")); TEST_ASSERT_NOT_NULL(strstr(response,"\"coordinate_kind\":\"rational\""));
    TEST_ASSERT_NOT_NULL(strstr(response,"\"residual\":\"0\"")); free(response);
}
static void test_http_failure_never_publishes_a_partial_root_list(void)
{
    const char *target = "/api/equations/solve?mode=numeric_scan&min=-2&max=2&precision=34";
    char *response = NULL; numforge_test_allocator_begin(0U);
    TEST_ASSERT_EQUAL(CALCULATOR_OK,numforge_web_session_request(&store,"POST",target,"x^2=1",&response));
    size_t calls = numforge_test_allocator_call_count(); numforge_test_allocator_end(); free(response);
    for (size_t i = 1U; i <= 80U; ++i) {
        response = NULL; numforge_test_allocator_begin(i <= 32U ? i : 1U+(calls-1U)*(i-32U)/48U);
        TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY,numforge_web_session_request(&store,"POST",target,"x^2=1",&response));
        numforge_test_allocator_end();
        if (response) { TEST_ASSERT_NULL(strstr(response,"\"ok\":true")); TEST_ASSERT_NOT_NULL(strstr(response,"numeric_equation_error")); }
        free(response);
    }
}
int main(void)
{
    UNITY_BEGIN(); RUN_TEST(test_ordered_roots_and_shared_endpoint_uniqueness);
    RUN_TEST(test_transcendental_references_at_precision_extremes);
    RUN_TEST(test_domain_gaps_and_rejected_brackets_do_not_become_roots);
    RUN_TEST(test_grid_is_explicitly_incomplete_and_limits_are_bounded);
    RUN_TEST(test_quadratic_contacts_keep_exact_rational_ordinates);
    RUN_TEST(test_candidate_coordinates_preserve_large_common_offsets);
    RUN_TEST(test_failures_preserve_outputs_and_inherit_the_outer_budget);
    RUN_TEST(test_http_scan_contract_and_full_coordinates);
    RUN_TEST(test_http_failure_never_publishes_a_partial_root_list);
    return UNITY_END();
}
