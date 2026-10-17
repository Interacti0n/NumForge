#include "equation.h"
#include "../src/internal/numforge_alloc.h"
#include <unity.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CalculatorContext context;
static CalculatorError error;
static BigRational *coefficients[3];
static ApplicationEquationSolution *solution;
static BigRationalComplex *root;

void setUp(void)
{
    calculator_context_init(&context);
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_TRUE(numforge_alloc_stats_track(true));
#endif
    for (size_t i = 0; i < 3U; ++i) {
        coefficients[i] = bigrational_create();
        TEST_ASSERT_NOT_NULL(coefficients[i]);
    }
}
void tearDown(void)
{
    numforge_test_allocator_end(); numforge_budget_end();
    application_equation_destroy(solution); solution = NULL;
    bigrationalcomplex_destroy(root); root = NULL;
    for (size_t i = 0; i < 3U; ++i) {
        bigrational_destroy(coefficients[i]); coefficients[i] = NULL;
    }
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_TRUE(numforge_alloc_stats_complete());
    TEST_ASSERT_EQUAL_UINT64(0U, numforge_alloc_stats_live());
    TEST_ASSERT_TRUE(numforge_alloc_stats_track(false));
#endif
}
static void fraction(BigRational *value, const char *numerator, const char *denominator)
{
    BigInt *n = bigint_create(), *d = bigint_create();
    TEST_ASSERT_NOT_NULL(n); TEST_ASSERT_NOT_NULL(d);
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(n, numerator));
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(d, denominator));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_set_fraction(value, n, d));
    bigint_destroy(n); bigint_destroy(d);
}
static void integer(BigRational *value, int64_t number)
{
    char buffer[32]; (void)snprintf(buffer, sizeof(buffer), "%" PRId64, number);
    fraction(value, buffer, "1");
}
static void equation(int64_t a, int64_t b, int64_t c)
{
    integer(coefficients[0], a); integer(coefficients[1], b); integer(coefficients[2], c);
    application_equation_destroy(solution); solution = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_equation_solve(&solution,
        coefficients[0], coefficients[1], coefficients[2], &context, &error));
    TEST_ASSERT_EQUAL(CALCULATOR_OK, error.status);
}
static void rational_text(const BigRational *value, const char *expected)
{
    char *text = NULL;
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_to_string(value, &text));
    TEST_ASSERT_EQUAL_STRING(expected, text); free(text);
}
static void root_parts(size_t index, const char *real, const char *imaginary)
{
    bigrationalcomplex_destroy(root); root = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_equation_root_exact(&root, solution, index, &context, &error));
    BigRational *part = bigrational_create(); TEST_ASSERT_NOT_NULL(part);
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK, bigrationalcomplex_get_real(part, root)); rational_text(part, real);
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK, bigrationalcomplex_get_imaginary(part, root)); rational_text(part, imaginary);
    bigrational_destroy(part);
}
static void test_degree_reduction_and_multiplicity(void)
{
    equation(0, 0, 0); TEST_ASSERT_EQUAL(APPLICATION_EQUATION_ALL, solution->kind);
    TEST_ASSERT_EQUAL(0U, solution->count); TEST_ASSERT_NULL(solution->center);
    equation(0, 0, 7); TEST_ASSERT_EQUAL(APPLICATION_EQUATION_NONE, solution->kind);
    TEST_ASSERT_EQUAL(0U, solution->degree); TEST_ASSERT_EQUAL(0U, solution->count);
    equation(0, 3, -2); TEST_ASSERT_EQUAL(1U, solution->degree);
    TEST_ASSERT_EQUAL(1U, solution->count); TEST_ASSERT_EQUAL(1U, solution->multiplicity[0]);
    TEST_ASSERT_NULL(solution->discriminant); root_parts(0U, "2/3", "0");
    equation(4, 12, 9); TEST_ASSERT_EQUAL(2U, solution->degree);
    TEST_ASSERT_EQUAL(1U, solution->count); TEST_ASSERT_EQUAL(2U, solution->multiplicity[0]);
    rational_text(solution->discriminant, "0"); root_parts(0U, "-3/2", "0");
    equation(1, 0, 0); root_parts(0U, "0", "0");
}
static void test_fractional_and_complex_roots(void)
{
    fraction(coefficients[0], "1", "2"); fraction(coefficients[1], "-5", "6"); fraction(coefficients[2], "1", "3");
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_equation_solve(&solution, coefficients[0], coefficients[1], coefficients[2], &context, &error));
    root_parts(0U, "2/3", "0"); root_parts(1U, "1", "0");
    equation(-6, 5, -1); root_parts(0U, "1/3", "0"); root_parts(1U, "1/2", "0");
    equation(9, -6, 5); TEST_ASSERT_TRUE(solution->imaginary);
    root_parts(0U, "1/3", "-2/3"); root_parts(1U, "1/3", "2/3");
    equation(1, 0, 1); root_parts(0U, "0", "-1"); root_parts(1U, "0", "1");
}
static void test_irrational_roots_stay_exact_algebraic_values(void)
{
    equation(1, 0, -2); TEST_ASSERT_NULL(solution->offset); TEST_ASSERT_FALSE(solution->imaginary);
    rational_text(solution->center, "0"); rational_text(solution->radicand, "2");
    rational_text(solution->discriminant, "8"); TEST_ASSERT_EQUAL(2U, solution->count);
    TEST_ASSERT_EQUAL(CALCULATOR_NOT_IMPLEMENTED, application_equation_root_exact(&root, solution, 0U, &context, &error));
    TEST_ASSERT_NULL(root);
    equation(3, 1, 1); TEST_ASSERT_NULL(solution->offset); TEST_ASSERT_TRUE(solution->imaginary);
    rational_text(solution->center, "-1/6"); rational_text(solution->radicand, "11/36");
    rational_text(solution->discriminant, "-11");
    /* Discriminant is exactly one despite cancellation of large products. */
    context.division_scale = 1;
    fraction(coefficients[0], "1", "1");
    fraction(coefficients[1], "-200000000000000000001", "1");
    fraction(coefficients[2], "10000000000000000000100000000000000000000", "1");
    application_equation_destroy(solution); solution = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_equation_solve(&solution, coefficients[0], coefficients[1], coefficients[2], &context, &error));
    rational_text(solution->discriminant, "1");
    root_parts(0U, "100000000000000000000", "0"); root_parts(1U, "100000000000000000001", "0");
}
static void check_original_polynomial(size_t index)
{
    bigrationalcomplex_destroy(root); root = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_equation_root_exact(&root, solution, index, &context, &error));
    BigRationalComplex *value = bigrationalcomplex_create(), *coefficient = bigrationalcomplex_create();
    BigRational *zero = bigrational_create(), *part = bigrational_create();
    TEST_ASSERT_NOT_NULL(value); TEST_ASSERT_NOT_NULL(coefficient); TEST_ASSERT_NOT_NULL(zero); TEST_ASSERT_NOT_NULL(part);
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK, bigrationalcomplex_set_parts(value, coefficients[0], zero));
    for (size_t i = 1U; i < 3U; ++i) {
        TEST_ASSERT_EQUAL(BIGCOMPLEX_OK, bigrationalcomplex_mul(value, value, root));
        TEST_ASSERT_EQUAL(BIGCOMPLEX_OK, bigrationalcomplex_set_parts(coefficient, coefficients[i], zero));
        TEST_ASSERT_EQUAL(BIGCOMPLEX_OK, bigrationalcomplex_add(value, value, coefficient));
    }
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK, bigrationalcomplex_get_real(part, value)); rational_text(part, "0");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK, bigrationalcomplex_get_imaginary(part, value)); rational_text(part, "0");
    bigrationalcomplex_destroy(value); bigrationalcomplex_destroy(coefficient); bigrational_destroy(zero); bigrational_destroy(part);
}
static void test_known_root_families_and_original_expression_residuals(void)
{
    for (int64_t scale = -2; scale <= 2; ++scale) if (scale) {
        for (int64_t p = -2; p <= 2; ++p) for (int64_t q = p; q <= 2; ++q) {
            equation(scale, -scale*(p+q), scale*p*q);
            char first[32], second[32];
            (void)snprintf(first, sizeof(first), "%" PRId64, p); (void)snprintf(second, sizeof(second), "%" PRId64, q);
            root_parts(0U, first, "0"); check_original_polynomial(0U);
            TEST_ASSERT_EQUAL(p == q ? 1U : 2U, solution->count);
            if (p != q) { root_parts(1U, second, "0"); check_original_polynomial(1U); }
        }
        for (int64_t re = -2; re <= 2; ++re) for (int64_t im = 1; im <= 2; ++im) {
            equation(scale, -2*scale*re, scale*(re*re+im*im));
            TEST_ASSERT_TRUE(solution->imaginary); TEST_ASSERT_EQUAL(2U, solution->count);
            check_original_polynomial(0U); check_original_polynomial(1U);
        }
    }
}
static void test_inputs_are_borrowed_and_context_is_always_validated(void)
{
    integer(coefficients[0], 1);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_equation_solve(&solution, coefficients[0], coefficients[0], coefficients[0], &context, &error));
    rational_text(coefficients[0], "1"); rational_text(solution->radicand, "3/4");
    ApplicationEquationSolution *original = solution;
    TEST_ASSERT_EQUAL(CALCULATOR_NULL_ARGUMENT, application_equation_solve(&solution, NULL, coefficients[1], coefficients[2], &context, &error));
    TEST_ASSERT_EQUAL_PTR(original, solution);
    context.time_limit_ms = -1;
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT, application_equation_solve(&solution, coefficients[1], coefficients[1], coefficients[1], &context, &error));
    TEST_ASSERT_EQUAL_PTR(original, solution); calculator_context_init(&context);
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT, application_equation_root_exact(&root, solution, 2U, &context, &error));
    TEST_ASSERT_NULL(root);
    equation(0, 0, 0);
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT, application_equation_root_exact(&root, solution, 0U, &context, &error));
}
static void test_every_allocation_failure_is_atomic(void)
{
    const int64_t cases[][3] = {{0, 3, -2}, {1, -3, 2}, {3, 1, 1}};
    for (size_t k = 0; k < sizeof(cases)/sizeof(cases[0]); ++k) {
        equation(cases[k][0], cases[k][1], cases[k][2]);
        ApplicationEquationSolution *attempt = NULL, *original = solution;
        numforge_test_allocator_begin(0U);
        TEST_ASSERT_EQUAL(CALCULATOR_OK, application_equation_solve(&attempt, coefficients[0], coefficients[1], coefficients[2], &context, &error));
        size_t calls = numforge_test_allocator_call_count(); numforge_test_allocator_end(); application_equation_destroy(attempt);
        TEST_ASSERT_TRUE(calls > 0U);
        for (size_t i = 1U; i <= calls; ++i) {
            numforge_test_allocator_begin(i);
            TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY, application_equation_solve(&solution, coefficients[0], coefficients[1], coefficients[2], &context, &error));
            numforge_test_allocator_end(); TEST_ASSERT_EQUAL_PTR(original, solution);
        }
    }
    equation(9, -6, 5); root_parts(0U, "1/3", "-2/3");
    BigRationalComplex *attempt = NULL, *original_root = root;
    numforge_test_allocator_begin(0U);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_equation_root_exact(&attempt, solution, 1U, &context, &error));
    size_t calls = numforge_test_allocator_call_count(); numforge_test_allocator_end(); bigrationalcomplex_destroy(attempt);
    for (size_t i = 1U; i <= calls; ++i) {
        numforge_test_allocator_begin(i);
        TEST_ASSERT_EQUAL(CALCULATOR_OUT_OF_MEMORY, application_equation_root_exact(&root, solution, 1U, &context, &error));
        numforge_test_allocator_end(); TEST_ASSERT_EQUAL_PTR(original_root, root);
    }
}
static void test_deadline_and_outer_memory_budget_preserve_destinations(void)
{
    equation(1, -3, 2); root_parts(0U, "1", "0");
    ApplicationEquationSolution *original = solution; BigRationalComplex *original_root = root;
    for (size_t checkpoint = 1U; checkpoint <= 60U; ++checkpoint) {
        ApplicationEquationSolution *attempt = original;
        TEST_ASSERT_TRUE(numforge_budget_begin(5000U, SIZE_MAX, SIZE_MAX));
        numforge_test_budget_expire_after(checkpoint);
        CalculatorStatus status = application_equation_solve(&attempt, coefficients[0], coefficients[1], coefficients[2], &context, &error);
        numforge_budget_end();
        if (checkpoint == 1U) TEST_ASSERT_EQUAL(CALCULATOR_TIME_LIMIT, status);
        if (status == CALCULATOR_OK) application_equation_destroy(attempt);
        else { TEST_ASSERT_EQUAL(CALCULATOR_TIME_LIMIT, status); TEST_ASSERT_EQUAL_PTR(original, attempt); }
        BigRationalComplex *value = original_root;
        TEST_ASSERT_TRUE(numforge_budget_begin(5000U, SIZE_MAX, SIZE_MAX));
        numforge_test_budget_expire_after(checkpoint);
        status = application_equation_root_exact(&value, solution, 0U, &context, &error);
        numforge_budget_end();
        if (checkpoint == 1U) TEST_ASSERT_EQUAL(CALCULATOR_TIME_LIMIT, status);
        if (status == CALCULATOR_OK) bigrationalcomplex_destroy(value);
        else { TEST_ASSERT_EQUAL(CALCULATOR_TIME_LIMIT, status); TEST_ASSERT_EQUAL_PTR(original_root, value); }
    }
    TEST_ASSERT_TRUE(numforge_budget_begin(5000U, 1U, 1U));
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_TOO_LARGE, application_equation_solve(&solution, coefficients[0], coefficients[1], coefficients[2], &context, &error));
    TEST_ASSERT_EQUAL_PTR(original, solution); TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_MEMORY, numforge_budget_failure()); numforge_budget_end();
}
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_degree_reduction_and_multiplicity);
    RUN_TEST(test_fractional_and_complex_roots);
    RUN_TEST(test_irrational_roots_stay_exact_algebraic_values);
    RUN_TEST(test_known_root_families_and_original_expression_residuals);
    RUN_TEST(test_inputs_are_borrowed_and_context_is_always_validated);
    RUN_TEST(test_every_allocation_failure_is_atomic);
    RUN_TEST(test_deadline_and_outer_memory_budget_preserve_destinations);
    return UNITY_END();
}
