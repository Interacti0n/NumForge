#include "numeric_root.h"
#include "session_api.h"
#include "expression.h"
#include "../src/internal/numforge_alloc.h"
#include <unity.h>
#include <stdlib.h>
#include <string.h>
#include "../benchmarks/numeric_root_cases.h"

static CalculatorContext context;
static CalculatorError error;
static BigRational *minimum, *maximum;
static ApplicationNumericRoot *root;
static ApplicationClientStore store;
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
    application_numeric_root_destroy(root); root = NULL;
    application_client_store_destroy(&store);
    bigrational_destroy(minimum); bigrational_destroy(maximum);
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_TRUE(numforge_alloc_stats_complete()); TEST_ASSERT_EQUAL_UINT64(0U, numforge_alloc_stats_live());
    TEST_ASSERT_TRUE(numforge_alloc_stats_track(false));
#endif
}
static void bound(BigRational *result, const char *text)
{
    BigDecimal *value = bigdecimal_create(); TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, text));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_from_bigdecimal(result, value)); bigdecimal_destroy(value);
}
static void solve(const char *input, const char *low, const char *high, ApplicationNumericRootKind kind)
{
    application_numeric_root_destroy(root); root = NULL; bound(minimum, low); bound(maximum, high);
    TEST_ASSERT_EQUAL_MESSAGE(CALCULATOR_OK, application_numeric_root_solve(&root, input, minimum, maximum, &context, &error), input);
    TEST_ASSERT_EQUAL_MESSAGE(kind, root->kind, input); TEST_ASSERT_TRUE(root->evaluations <= 192U); TEST_ASSERT_TRUE(root->iterations <= 96U);
}
static void near(const char *text)
{
    BigDecimal *value = bigdecimal_create(), *expected = bigdecimal_create(), *tolerance = bigdecimal_create(); int order = 1;
    TEST_ASSERT_NOT_NULL(value); TEST_ASSERT_NOT_NULL(expected); TEST_ASSERT_NOT_NULL(tolerance);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_to_bigdecimal(value, root->x, 60, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(expected, text));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_sub(value, value, expected)); TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_abs(value, value));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(tolerance, "1E-12"));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_compare(&order, value, tolerance)); TEST_ASSERT_TRUE(order <= 0);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_compare(&order, root->residual, tolerance)); TEST_ASSERT_TRUE(order <= 0);
    bigdecimal_destroy(value); bigdecimal_destroy(expected); bigdecimal_destroy(tolerance);
}
static void test_roots_use_original_high_precision_expressions(void)
{
    solve("cos(x)=x", "0", "1", APPLICATION_ROOT_FOUND); near("0.7390851332151606416553120876738734");
    TEST_ASSERT_FALSE(root->exact_zero);
    solve("exp(x)=2", "0", "2", APPLICATION_ROOT_FOUND); near("0.6931471805599453094172321214581766");
    solve("sqrt(x)=2", "0", "5", APPLICATION_ROOT_FOUND); near("4");
    solve("sin(x)=0", "-1", "1", APPLICATION_ROOT_FOUND); near("0");
    solve("x=1/3", "0", "1", APPLICATION_ROOT_FOUND); near("0.3333333333333333333333333333333333");
    solve("x=1E100", "0", "2E100", APPLICATION_ROOT_FOUND); near("1E100"); TEST_ASSERT_TRUE(root->exact_zero);
    solve("x=2", "2", "3", APPLICATION_ROOT_FOUND); near("2"); TEST_ASSERT_TRUE(root->exact_zero);
}
static void test_unbracketed_domains_and_jumps_do_not_claim_no_roots(void)
{
    solve("(x-0.1234)^2=0", "0", "1", APPLICATION_ROOT_NOT_BRACKETED); TEST_ASSERT_NULL(root->x);
    solve("1E-100=0", "0", "1", APPLICATION_ROOT_NOT_BRACKETED);
    solve("1/x=0", "-1", "1", APPLICATION_ROOT_DOMAIN_GAP); TEST_ASSERT_NULL(root->residual);
    solve("sqrt(x)=2", "-1", "5", APPLICATION_ROOT_DOMAIN_GAP);
    solve("floor(x)=1/2", "-1", "1", APPLICATION_ROOT_ITERATION_LIMIT); TEST_ASSERT_NULL(root->x);
    solve("1/(x-0.1234)=0", "0", "1", APPLICATION_ROOT_ITERATION_LIMIT);
    solve("1E-100/(x-0.1234)=0", "0", "1", APPLICATION_ROOT_ITERATION_LIMIT);
    solve("floor(x)*1E-100=5E-101", "-1", "1", APPLICATION_ROOT_ITERATION_LIMIT);
    solve("round(sin(x);0)=0", "-1", "1", APPLICATION_ROOT_PRECISION_LIMIT);
}
static void test_audit_reference_corpus_at_all_supported_precision_extremes(void)
{
    const int precisions[] = {16, 34, 100};
    for (size_t p = 0U; p < sizeof(precisions)/sizeof(precisions[0]); ++p) {
        context.division_scale = precisions[p];
        for (size_t i = 0U; i < sizeof(numeric_root_cases)/sizeof(numeric_root_cases[0]); ++i) {
            const NumericRootCase *scene = &numeric_root_cases[i];
            solve(scene->equation, scene->lower, scene->upper, scene->outcome);
            if (scene->reference) near(scene->reference);
            else { TEST_ASSERT_NULL(root->x); TEST_ASSERT_NULL(root->residual); }
        }
    }
}
static void domain(const char *input, const char *low, const char *high, bool expected)
{
    ApplicationExpression *expression = NULL; bool verified = !expected;
    bound(minimum, low); bound(maximum, high);
    TEST_ASSERT_EQUAL_MESSAGE(CALCULATOR_OK, application_expression_prepare(&expression, input, "x", NULL, &context, &error), input);
    TEST_ASSERT_EQUAL_MESSAGE(CALCULATOR_OK, application_expression_domain(&verified, expression, minimum, maximum), input);
    TEST_ASSERT_EQUAL_MESSAGE(expected, verified, input); application_expression_destroy(expression);
}
static void test_interval_domains_preserve_holes_and_allow_safe_compositions(void)
{
    domain("1/(x-0.1234)", "0", "1", false);
    domain("0/(x-0.1234)", "0", "1", false);
    domain("(x-0.1234)/(x-0.1234)", "0", "1", false);
    domain("1/abs(x-0.1234)", "0", "1", false);
    domain("1/(x-0.1234)^2", "0", "1", false);
    domain("1/(x-0.1234)", "0.2", "1", true);
    domain("1/(x-0.1234)", "-1", "0", true);
    domain("sqrt(x)", "-1", "1", false);
    domain("sqrt(x)", "0", "1", true);
    domain("ln(x)", "0", "1", false);
    domain("ln(x)", "0.0001", "1", true);
    domain("log(x)", "0.0001", "1", true);
    domain("log(x)", "0", "1", false);
    domain("ln(sqrt(x))", "0.5", "1", true);
    domain("ln(ln(x))", "2", "3", true);
    domain("ln(ln(x))", "0.5", "2", false);
    domain("sqrt(ln(x))", "2", "3", true);
    domain("exp(sin(x))+cos(x)", "-100", "100", true);
    domain("1/(π-x)", "0", "1", true);
    domain("1/(e-x)", "0", "1", true);
    domain("1/(φ-x)", "0", "0.5", true);
    domain("ln(exp(x))", "-1", "1", false);
    domain("abs(x)*x^3", "-1", "1", true);
    domain("(x-0.1234)^(-2)", "0", "1", false);
    domain("(x-0.1234)^(-2)", "0.2", "1", true);
    domain("pow(x;3)", "-1", "1", true);
    domain("x^100", "-1", "1", true);
    domain("x^x", "0.5", "1", true);
    domain("x^0.5", "-1", "1", false);
    domain("tan(x)", "1", "2", false);
    domain("floor(x)+x", "0", "1", false);
    domain("round(sin(x);0)", "-1", "1", false);
    domain("1/(abs(x)+1)", "-1", "1", true);
    domain("1/(x^2+1)", "-1", "1", true);
    domain("1/(x*x+1)", "-1", "1", false); /* dependency overestimate is honest */
}
static void test_candidate_domain_verification_prevents_false_roots(void)
{
    const int precisions[] = {16, 34, 100};
    for (size_t i = 0U; i < sizeof(precisions)/sizeof(precisions[0]); ++i) {
        context.division_scale = precisions[i];
        solve("x-0.1234+1E-30*(x-0.1234)/abs(x-0.1234)=0", "0", "1", APPLICATION_ROOT_DOMAIN_UNVERIFIED);
        TEST_ASSERT_NULL(root->x); TEST_ASSERT_NULL(root->residual);
        solve("x-0.1234+1E-30*sign(x-0.1234)=0", "0", "1", APPLICATION_ROOT_DOMAIN_UNVERIFIED);
        solve("1/(x-0.1234)=3", "0.3", "0.6", APPLICATION_ROOT_FOUND); near("0.4567333333333333333333333333333333");
        solve("1/(0.1234-x)=-3", "0.3", "0.6", APPLICATION_ROOT_FOUND); near("0.4567333333333333333333333333333333");
        solve("1/sqrt(x)=3", "0.01", "1", APPLICATION_ROOT_FOUND); near("0.1111111111111111111111111111111111");
        solve("ln(sqrt(x))=1", "1", "10", APPLICATION_ROOT_FOUND); near("7.3890560989306502272304274605750078");
        solve("(x-0.1234)^(-1)=3", "0.3", "0.6", APPLICATION_ROOT_FOUND); near("0.4567333333333333333333333333333333");
        solve("pow(x;3)=2", "0", "2", APPLICATION_ROOT_FOUND); near("1.259921049894873164767210607278228351");
    }
    /* An actual typed exact zero is valid even if a neighboring point has a
     * hole or unsupported function. No global continuity claim is made. */
    solve("floor(x)=0", "0", "1", APPLICATION_ROOT_FOUND); TEST_ASSERT_TRUE(root->exact_zero);
    solve("1/(x-1)=2", "0", "1.5", APPLICATION_ROOT_FOUND); near("1.5"); TEST_ASSERT_TRUE(root->exact_zero);
}
static void test_domain_verification_allocation_failures_are_atomic(void)
{
    ApplicationExpression *expression = NULL;
    bound(minimum, "2"); bound(maximum, "3");
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_expression_prepare(&expression, "ln(sqrt(x))/(x*x+1)", "x", NULL, &context, &error));
    bool verified = false; numforge_test_allocator_begin(0U);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_expression_domain(&verified, expression, minimum, maximum));
    size_t calls = numforge_test_allocator_call_count(); numforge_test_allocator_end(); TEST_ASSERT_TRUE(verified);
    for (size_t i = 1U; i <= calls; ++i) {
        verified = false; numforge_test_allocator_begin(i);
        TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY, application_expression_domain(&verified, expression, minimum, maximum));
        numforge_test_allocator_end(); TEST_ASSERT_FALSE(verified);
    }
    verified = false; TEST_ASSERT_TRUE(numforge_budget_begin(5000U, 1U, 1U));
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_TOO_LARGE, application_expression_domain(&verified, expression, minimum, maximum));
    TEST_ASSERT_FALSE(verified); numforge_budget_end();
    for (size_t i = 1U; i <= 40U; ++i) {
        verified = false; numforge_test_budget_expire_after(i);
        TEST_ASSERT_TRUE(numforge_budget_begin(5000U, CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION));
        CalculatorStatus status = application_expression_domain(&verified, expression, minimum, maximum);
        TEST_ASSERT_TRUE(status == CALCULATOR_OK || status == CALCULATOR_TIME_LIMIT);
        if (status != CALCULATOR_OK) TEST_ASSERT_FALSE(verified);
        numforge_budget_end(); numforge_test_allocator_end();
    }
    application_expression_destroy(expression);
}
static void test_invalid_inputs_and_read_only_failures(void)
{
    solve("x=0", "-1", "1", APPLICATION_ROOT_FOUND); ApplicationNumericRoot *original = root;
    TEST_ASSERT_EQUAL(CALCULATOR_SYNTAX_ERROR, application_numeric_root_solve(&root, "x=0=1", minimum, maximum, &context, &error));
    TEST_ASSERT_EQUAL_PTR(original, root);
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT, application_numeric_root_solve(&root, "rand()=x", minimum, maximum, &context, &error));
    TEST_ASSERT_EQUAL(CALCULATOR_UNDEFINED_VARIABLE, application_numeric_root_solve(&root, "y=x", minimum, maximum, &context, &error));
    TEST_ASSERT_EQUAL(CALCULATOR_UNDEFINED_ANSWER, application_numeric_root_solve(&root, "x=ans", minimum, maximum, &context, &error));
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT, application_numeric_root_solve(&root, "x=0", maximum, minimum, &context, &error));
    context.division_scale = 15;
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT, application_numeric_root_solve(&root, "x=0", minimum, maximum, &context, &error));
    TEST_ASSERT_EQUAL_PTR(original, root);
}
static void test_allocation_failures_and_outer_budgets_are_atomic(void)
{
    solve("x=0", "-1", "1", APPLICATION_ROOT_FOUND); ApplicationNumericRoot *original = root, *attempt = NULL;
    numforge_test_allocator_begin(0U);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_numeric_root_solve(&attempt, "x=0", minimum, maximum, &context, &error));
    size_t calls = numforge_test_allocator_call_count(); numforge_test_allocator_end(); application_numeric_root_destroy(attempt);
    for (size_t i = 1U; i <= calls; ++i) {
        numforge_test_allocator_begin(i);
        TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY, application_numeric_root_solve(&root, "x=0", minimum, maximum, &context, &error));
        numforge_test_allocator_end(); TEST_ASSERT_EQUAL_PTR(original, root);
    }
    for (size_t i = 1U; i <= 80U; ++i) {
        TEST_ASSERT_TRUE(numforge_budget_begin(5000U, SIZE_MAX, SIZE_MAX)); numforge_test_budget_expire_after(i);
        attempt = original; CalculatorStatus status = application_numeric_root_solve(&attempt, "cos(x)=x", minimum, maximum, &context, &error);
        if (i == 1U) TEST_ASSERT_EQUAL(CALCULATOR_TIME_LIMIT, status);
        if (status == CALCULATOR_OK) application_numeric_root_destroy(attempt);
        else { TEST_ASSERT_EQUAL(CALCULATOR_TIME_LIMIT, status); TEST_ASSERT_EQUAL_PTR(original, attempt); }
        numforge_budget_end();
    }
    TEST_ASSERT_TRUE(numforge_budget_begin(5000U, 1U, 1U));
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_TOO_LARGE, application_numeric_root_solve(&root, "cos(x)=x", minimum, maximum, &context, &error));
    TEST_ASSERT_EQUAL_PTR(original, root); numforge_budget_end();
}
static void test_iterative_function_allocation_failures_are_atomic(void)
{
    solve("x=0", "-1", "1", APPLICATION_ROOT_FOUND); ApplicationNumericRoot *original = root, *attempt = NULL;
    numforge_test_allocator_begin(0U);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_numeric_root_solve(&attempt, "cos(x)=x", minimum, maximum, &context, &error));
    size_t calls = numforge_test_allocator_call_count(); numforge_test_allocator_end(); application_numeric_root_destroy(attempt);
    for (size_t i = 0U; i < 24U; ++i) {
        numforge_test_allocator_begin(1U+(calls-1U)*i/23U);
        TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY, application_numeric_root_solve(&root, "cos(x)=x", minimum, maximum, &context, &error));
        numforge_test_allocator_end(); TEST_ASSERT_EQUAL_PTR(original, root);
    }
}
static void test_numeric_http_contract_and_diagnostics(void)
{
    char *response = NULL;
    const char *target = "/api/equations/solve?mode=numeric&min=0&max=1&precision=16";
    TEST_ASSERT_EQUAL(CALCULATOR_OK, numforge_web_session_request(&store, "POST", target, "cos(x)=x", &response));
    TEST_ASSERT_NOT_NULL(strstr(response, "\"kind\":\"numeric\",\"outcome\":\"found\",\"complete\":false,\"count\":1"));
    TEST_ASSERT_NOT_NULL(strstr(response, "\"approximation\":\"0.739085133215"));
    TEST_ASSERT_NOT_NULL(strstr(response, "\"residual\":")); TEST_ASSERT_NULL(strstr(response, "\"exact\":")); free(response); response = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, numforge_web_session_request(&store, "POST", target, "(x-0.1234)^2=0", &response));
    TEST_ASSERT_NOT_NULL(strstr(response, "\"outcome\":\"not_bracketed\"")); TEST_ASSERT_NOT_NULL(strstr(response, "\"roots\":[]")); free(response); response = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, numforge_web_session_request(&store, "POST", target, "x-0.1234+1E-30*(x-0.1234)/abs(x-0.1234)=0", &response));
    TEST_ASSERT_NOT_NULL(strstr(response, "\"outcome\":\"domain_unverified\",\"complete\":false,\"count\":0"));
    TEST_ASSERT_NOT_NULL(strstr(response, "\"roots\":[]")); TEST_ASSERT_NULL(strstr(response, "\"residual\":")); free(response); response = NULL;
    const char *invalid[] = {"/api/equations/solve?mode=numeric", "/api/equations/solve?mode=numeric&min=0&max=0",
        "/api/equations/solve?mode=numeric&min=2&max=1", "/api/equations/solve?mode=numeric&min=bad&max=1",
        "/api/equations/solve?mode=numeric&min=0&max=1&precision=15", "/api/equations/solve?mode=numeric&min=0&max=1&precision=101",
        "/api/equations/solve?mode=numeric&min=0&max=1&angle=deg", "/api/equations/solve?mode=numeric&min=0&max=1&client=abc"};
    for (size_t i = 0U; i < sizeof(invalid)/sizeof(invalid[0]); ++i) {
        TEST_ASSERT_NOT_EQUAL(CALCULATOR_OK, numforge_web_session_request(&store, "POST", invalid[i], "cos(x)=x", &response));
        TEST_ASSERT_NOT_NULL(strstr(response, "\"ok\":false")); free(response); response = NULL;
    }
    TEST_ASSERT_EQUAL(CALCULATOR_UNDEFINED_VARIABLE, numforge_web_session_request(&store, "POST", target, "cos(x)=y", &response));
    TEST_ASSERT_NOT_NULL(strstr(response, "\"code\":\"numeric_equation_error\",\"column\":8")); free(response);
    static const ApplicationClientStore empty = {0}; TEST_ASSERT_EQUAL_MEMORY(&empty, &store, sizeof(store));
}
static void test_numeric_http_allocation_failures_do_not_publish_roots(void)
{
    const char *target = "/api/equations/solve?mode=numeric&min=-1&max=1"; char *response = NULL;
    numforge_test_allocator_begin(0U);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, numforge_web_session_request(&store, "POST", target, "x=0", &response));
    size_t calls = numforge_test_allocator_call_count(); numforge_test_allocator_end(); free(response);
    for (size_t i = 1U; i <= calls; ++i) {
        response = NULL; numforge_test_allocator_begin(i);
        TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY, numforge_web_session_request(&store, "POST", target, "x=0", &response));
        numforge_test_allocator_end();
        if (response) {
            TEST_ASSERT_NULL(strstr(response, "\"ok\":true"));
            TEST_ASSERT_NOT_NULL(strstr(response, "\"code\":\"numeric_equation_error\""));
        }
        free(response);
    }
    response = NULL; TEST_ASSERT_TRUE(numforge_budget_begin(5000U, 1U, 1U));
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_TOO_LARGE, numforge_web_session_request(&store, "POST", target, "x=0", &response));
    TEST_ASSERT_NULL(response); numforge_budget_end();
}
int main(void)
{
    UNITY_BEGIN(); RUN_TEST(test_roots_use_original_high_precision_expressions);
    RUN_TEST(test_unbracketed_domains_and_jumps_do_not_claim_no_roots);
    RUN_TEST(test_audit_reference_corpus_at_all_supported_precision_extremes);
    RUN_TEST(test_interval_domains_preserve_holes_and_allow_safe_compositions);
    RUN_TEST(test_candidate_domain_verification_prevents_false_roots);
    RUN_TEST(test_domain_verification_allocation_failures_are_atomic);
    RUN_TEST(test_invalid_inputs_and_read_only_failures); RUN_TEST(test_allocation_failures_and_outer_budgets_are_atomic);
    RUN_TEST(test_iterative_function_allocation_failures_are_atomic);
    RUN_TEST(test_numeric_http_contract_and_diagnostics); RUN_TEST(test_numeric_http_allocation_failures_do_not_publish_roots);
    return UNITY_END();
}
