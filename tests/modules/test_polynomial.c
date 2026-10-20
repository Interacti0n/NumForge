#include <numforge/polynomial.h>
#include <numforge/runtime.h>
#include "src/runtime/numforge_alloc.h"
#include <unity.h>
#include <stdlib.h>
#include <string.h>

static BigRational *coefficients[3];
static NumForgePolynomialSolution *solution;
static BigRationalComplex *root;
void setUp(void)
{
    for (size_t i = 0U; i < 3U; ++i) {
        coefficients[i] = bigrational_create(); TEST_ASSERT_NOT_NULL(coefficients[i]);
    }
}
void tearDown(void)
{
    numforge_test_allocator_end(); numforge_test_budget_expire_after(0U); numforge_budget_end();
    numforge_polynomial_destroy(solution); solution = NULL;
    bigrationalcomplex_destroy(root); root = NULL;
    for (size_t i = 0U; i < 3U; ++i) { bigrational_destroy(coefficients[i]); coefficients[i] = NULL; }
}
static void integer(size_t i, const char *text)
{
    BigInt *value = bigint_create(); TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(value, text));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_from_bigint(coefficients[i], value)); bigint_destroy(value);
}
static void solve(const char *a, const char *b, const char *c)
{
    integer(0U, a); integer(1U, b); integer(2U, c);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, numforge_polynomial_solve(&solution, coefficients[0], coefficients[1], coefficients[2]));
}
static void rational_text(const BigRational *value, const char *expected)
{
    char *text = NULL; TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_to_string(value, &text));
    TEST_ASSERT_EQUAL_STRING(expected, text); free(text);
}
static void root_parts(size_t index, const char *real, const char *imaginary)
{
    BigRational *part = bigrational_create(); TEST_ASSERT_NOT_NULL(part);
    bigrationalcomplex_destroy(root); root = NULL;
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, numforge_polynomial_root_exact(&root, solution, index));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK, bigrationalcomplex_get_real(part, root)); rational_text(part, real);
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK, bigrationalcomplex_get_imaginary(part, root)); rational_text(part, imaginary);
    bigrational_destroy(part);
}
static void test_real_rational_roots_and_multiplicity(void)
{
    solve("15", "16", "-7"); /* roots -7/5 and 1/3 */
    TEST_ASSERT_EQUAL(NUMFORGE_POLYNOMIAL_FINITE, numforge_polynomial_kind(solution));
    TEST_ASSERT_EQUAL_UINT(2U, numforge_polynomial_degree(solution));
    TEST_ASSERT_EQUAL_UINT(2U, numforge_polynomial_root_count(solution));
    TEST_ASSERT_EQUAL_UINT(1U, numforge_polynomial_root_multiplicity(solution, 0U));
    TEST_ASSERT_FALSE(numforge_polynomial_is_imaginary(solution));
    root_parts(0U, "-7/5", "0"); root_parts(1U, "1/3", "0");
    rational_text(numforge_polynomial_center(solution), "-8/15");
    rational_text(numforge_polynomial_radicand(solution), "169/225");
    rational_text(numforge_polynomial_rational_offset(solution), "13/15");
    rational_text(numforge_polynomial_discriminant(solution), "676");
    numforge_polynomial_destroy(solution); solution = NULL;
    solve("-1", "2", "-1");
    TEST_ASSERT_EQUAL_UINT(1U, numforge_polynomial_root_count(solution));
    TEST_ASSERT_EQUAL_UINT(2U, numforge_polynomial_root_multiplicity(solution, 0U)); root_parts(0U, "1", "0");
}
static void test_irrational_and_complex_descriptors(void)
{
    solve("1", "0", "-2");
    rational_text(numforge_polynomial_center(solution), "0");
    rational_text(numforge_polynomial_radicand(solution), "2");
    TEST_ASSERT_NULL(numforge_polynomial_rational_offset(solution));
    TEST_ASSERT_FALSE(numforge_polynomial_is_imaginary(solution));
    TEST_ASSERT_EQUAL(BIGRATIONAL_INVALID_ARGUMENT, numforge_polynomial_root_exact(&root, solution, 0U)); TEST_ASSERT_NULL(root);
    numforge_polynomial_destroy(solution); solution = NULL;
    solve("9", "0", "1");
    TEST_ASSERT_TRUE(numforge_polynomial_is_imaginary(solution));
    root_parts(0U, "0", "-1/3"); root_parts(1U, "0", "1/3");
    numforge_polynomial_destroy(solution); solution = NULL;
    solve("1", "2", "3");
    TEST_ASSERT_TRUE(numforge_polynomial_is_imaginary(solution));
    rational_text(numforge_polynomial_center(solution), "-1");
    rational_text(numforge_polynomial_radicand(solution), "2");
    TEST_ASSERT_NULL(numforge_polynomial_rational_offset(solution));
}
static void test_degenerate_equations(void)
{
    solve("0", "2", "-1");
    TEST_ASSERT_EQUAL_UINT(1U, numforge_polynomial_degree(solution)); root_parts(0U, "1/2", "0");
    TEST_ASSERT_NULL(numforge_polynomial_discriminant(solution));
    numforge_polynomial_destroy(solution); solution = NULL;
    solve("0", "0", "0"); TEST_ASSERT_EQUAL(NUMFORGE_POLYNOMIAL_ALL, numforge_polynomial_kind(solution));
    TEST_ASSERT_EQUAL_UINT(0U, numforge_polynomial_root_count(solution)); TEST_ASSERT_NULL(numforge_polynomial_center(solution));
    numforge_polynomial_destroy(solution); solution = NULL;
    solve("0", "0", "1"); TEST_ASSERT_EQUAL(NUMFORGE_POLYNOMIAL_NONE, numforge_polynomial_kind(solution));
    TEST_ASSERT_EQUAL_UINT(0U, numforge_polynomial_degree(solution)); TEST_ASSERT_NULL(numforge_polynomial_radicand(solution));
    TEST_ASSERT_EQUAL(BIGRATIONAL_INVALID_ARGUMENT, numforge_polynomial_root_exact(&root, solution, 0U));
}
static void test_large_exact_coefficients_and_no_implicit_scope(void)
{
    char power[1002]; power[0] = '1'; memset(power + 1, '0', 1000U); power[1001] = '\0';
    solve("0", "1", "0"); numforge_polynomial_destroy(solution); solution = NULL;
    integer(1U, power); integer(2U, "1");
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, numforge_polynomial_solve(&solution, coefficients[0], coefficients[1], coefficients[2]));
    char expected[1005]; memcpy(expected, "-1/", 3U); memcpy(expected + 3U, power, sizeof(power));
    root_parts(0U, expected, "0");
    TEST_ASSERT_TRUE(numforge_budget_begin(0U, SIZE_MAX, SIZE_MAX));
    TEST_ASSERT_FALSE(numforge_budget_check());
    TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_TIME, numforge_budget_failure()); numforge_budget_end();
}
static void test_nulls_aliases_and_failure_preservation(void)
{
    integer(0U, "1");
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, numforge_polynomial_solve(&solution, coefficients[0], coefficients[0], coefficients[0]));
    rational_text(coefficients[0], "1"); rational_text(numforge_polynomial_radicand(solution), "3/4");
    NumForgePolynomialSolution *saved = solution;
    TEST_ASSERT_EQUAL(BIGRATIONAL_NULL_ARGUMENT, numforge_polynomial_solve(&solution, NULL, coefficients[1], coefficients[2]));
    TEST_ASSERT_EQUAL_PTR(saved, solution);
    TEST_ASSERT_EQUAL(BIGRATIONAL_NULL_ARGUMENT, numforge_polynomial_solve(NULL, coefficients[0], coefficients[1], coefficients[2]));
    TEST_ASSERT_EQUAL(BIGRATIONAL_NULL_ARGUMENT, numforge_polynomial_root_exact(NULL, solution, 0U));
    TEST_ASSERT_EQUAL(BIGRATIONAL_NULL_ARGUMENT, numforge_polynomial_root_exact(&root, NULL, 0U));
    TEST_ASSERT_EQUAL(BIGRATIONAL_INVALID_ARGUMENT, numforge_polynomial_root_exact(&root, solution, 2U));
    TEST_ASSERT_EQUAL_UINT(0U, numforge_polynomial_root_multiplicity(solution, 2U));
    TEST_ASSERT_EQUAL(NUMFORGE_POLYNOMIAL_INVALID, numforge_polynomial_kind(NULL));
    TEST_ASSERT_EQUAL_UINT(0U, numforge_polynomial_degree(NULL)); TEST_ASSERT_EQUAL_UINT(0U, numforge_polynomial_root_count(NULL));
    TEST_ASSERT_EQUAL_UINT(0U, numforge_polynomial_root_multiplicity(NULL, 0U));
    TEST_ASSERT_FALSE(numforge_polynomial_is_imaginary(NULL)); TEST_ASSERT_NULL(numforge_polynomial_center(NULL));
    TEST_ASSERT_NULL(numforge_polynomial_radicand(NULL)); TEST_ASSERT_NULL(numforge_polynomial_rational_offset(NULL));
    TEST_ASSERT_NULL(numforge_polynomial_discriminant(NULL)); numforge_polynomial_destroy(NULL);
}
static void test_resource_and_allocation_failures(void)
{
    solve("1", "-3", "2"); NumForgePolynomialSolution *saved = solution;
    for (int operation = 0; operation < 2; ++operation) {
        bool complete = false;
        for (size_t failure = 1U; failure < 4096U; ++failure) {
            NumForgePolynomialSolution *output = saved;
            numforge_test_allocator_begin(failure);
            BigRationalStatus status = operation == 0 ? numforge_polynomial_solve(&output, coefficients[0], coefficients[1], coefficients[2]) :
                numforge_polynomial_root_exact(&root, solution, 0U);
            bool injected = numforge_test_allocator_did_fail(); numforge_test_allocator_end();
            if (status == BIGRATIONAL_OK) {
                if (operation == 0) numforge_polynomial_destroy(output);
                else { bigrationalcomplex_destroy(root); root = NULL; }
            } else {
                TEST_ASSERT_EQUAL(BIGRATIONAL_OUT_OF_MEMORY, status); TEST_ASSERT_EQUAL_PTR(saved, output); TEST_ASSERT_NULL(root);
            }
            if (!injected) { complete = true; break; }
        }
        TEST_ASSERT_TRUE(complete);
    }
    TEST_ASSERT_TRUE(numforge_budget_begin(5000U, 0U, SIZE_MAX));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OUT_OF_MEMORY, numforge_polynomial_solve(&solution, coefficients[0], coefficients[1], coefficients[2]));
    TEST_ASSERT_EQUAL_PTR(saved, solution); TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_MEMORY, numforge_budget_failure()); numforge_budget_end();
    const size_t checkpoints[] = {1U, 32U, 128U};
    for (size_t i = 0U; i < sizeof(checkpoints)/sizeof(checkpoints[0]); ++i) {
        TEST_ASSERT_TRUE(numforge_budget_begin(5000U, SIZE_MAX, SIZE_MAX)); numforge_test_budget_expire_after(checkpoints[i]);
        TEST_ASSERT_EQUAL(BIGRATIONAL_OUT_OF_MEMORY, numforge_polynomial_solve(&solution, coefficients[0], coefficients[1], coefficients[2]));
        TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_TIME, numforge_budget_failure()); TEST_ASSERT_EQUAL_PTR(saved, solution); numforge_budget_end();
    }
}
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_real_rational_roots_and_multiplicity);
    RUN_TEST(test_irrational_and_complex_descriptors);
    RUN_TEST(test_degenerate_equations);
    RUN_TEST(test_large_exact_coefficients_and_no_implicit_scope);
    RUN_TEST(test_nulls_aliases_and_failure_preservation);
    RUN_TEST(test_resource_and_allocation_failures);
    return UNITY_END();
}
