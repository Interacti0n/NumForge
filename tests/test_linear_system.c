#include <numforge/matrix.h>
#include <numforge/runtime.h>
#include "numforge_alloc.h"
#include <unity.h>
#include <stdlib.h>
#include <string.h>

static NumForgeMatrix *a, *b;
static NumForgeLinearSolution *solution;
void setUp(void) {}
void tearDown(void)
{
    numforge_test_allocator_end(); numforge_test_budget_expire_after(0); numforge_budget_end();
    numforge_matrix_destroy(a); numforge_matrix_destroy(b); numforge_linear_solution_destroy(solution);
    a = b = NULL; solution = NULL;
}
static NumForgeMatrix *matrix(size_t rows, size_t columns, const char *const *texts)
{
    const BigRational **entries = malloc(rows * columns * sizeof(*entries));
    BigInt *top = bigint_create(), *bottom = bigint_create(); NumForgeMatrix *m = NULL;
    TEST_ASSERT_NOT_NULL(entries); TEST_ASSERT_NOT_NULL(top); TEST_ASSERT_NOT_NULL(bottom);
    for (size_t i = 0; i < rows * columns; ++i) {
        char buffer[2048]; const char *slash = strchr(texts[i], '/');
        size_t length = slash ? (size_t)(slash - texts[i]) : strlen(texts[i]);
        TEST_ASSERT_TRUE(length < sizeof(buffer)); memcpy(buffer, texts[i], length); buffer[length] = '\0';
        BigRational *v = bigrational_create(); TEST_ASSERT_NOT_NULL(v);
        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(top, buffer));
        TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(bottom, slash ? slash + 1 : "1"));
        TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_set_fraction(v, top, bottom)); entries[i] = v;
    }
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_OK, numforge_matrix_create(&m, rows, columns, entries));
    for (size_t i = 0; i < rows * columns; ++i) bigrational_destroy((BigRational *)entries[i]);
    free(entries); bigint_destroy(top); bigint_destroy(bottom); return m;
}
static void equals(const NumForgeMatrix *m, size_t row, size_t column, const char *expected)
{
    char *text = NULL; TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_to_string(numforge_matrix_entry(m, row, column), &text));
    TEST_ASSERT_EQUAL_STRING(expected, text); free(text);
}
static void analyze(void)
{ TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_OK, numforge_matrix_solve_general(&solution, a, b)); }
static void metadata(NumForgeLinearSolutionKind kind, size_t rank, size_t nullity)
{
    TEST_ASSERT_EQUAL(kind, numforge_linear_solution_kind(solution));
    TEST_ASSERT_EQUAL_UINT(rank, numforge_linear_solution_rank(solution));
    TEST_ASSERT_EQUAL_UINT(nullity, numforge_linear_solution_nullity(solution));
    size_t actual = SIZE_MAX;
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_OK, numforge_matrix_rank(&actual, a)); TEST_ASSERT_EQUAL_UINT(rank, actual);
    TEST_ASSERT_EQUAL_UINT(SIZE_MAX, numforge_linear_solution_pivot_column(solution, rank));
    TEST_ASSERT_EQUAL_UINT(SIZE_MAX, numforge_linear_solution_free_column(solution, nullity));
}
static void test_skipped_pivots_complete_family_and_owned_lifetime(void)
{
    const char *left[] = {"0", "2", "4", "0", "3", "6"}, *right[] = {"10", "15"};
    a = matrix(2, 3, left); b = matrix(2, 1, right); analyze(); metadata(NUMFORGE_LINEAR_INFINITE, 1, 2);
    TEST_ASSERT_EQUAL_UINT(1, numforge_linear_solution_pivot_column(solution, 0));
    TEST_ASSERT_EQUAL_UINT(0, numforge_linear_solution_free_column(solution, 0));
    TEST_ASSERT_EQUAL_UINT(2, numforge_linear_solution_free_column(solution, 1));
    const NumForgeMatrix *particular = numforge_linear_solution_particular(solution), *basis = numforge_linear_solution_nullspace(solution);
    equals(particular, 0, 0, "0"); equals(particular, 1, 0, "5"); equals(particular, 2, 0, "0");
    const char *expected[] = {"1", "0", "0", "-2", "0", "1"};
    for (size_t i = 0; i < 6; ++i) equals(basis, i / 2, i % 2, expected[i]);
    NumForgeMatrix *product = NULL;
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_OK, numforge_matrix_multiply(&product, a, particular));
    equals(product, 0, 0, "10"); equals(product, 1, 0, "15"); numforge_matrix_destroy(product); product = NULL;
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_OK, numforge_matrix_multiply(&product, a, basis));
    for (size_t i = 0; i < 4; ++i) equals(product, i / 2, i % 2, "0");
    numforge_matrix_destroy(product);
    numforge_matrix_destroy(a); a = NULL; numforge_matrix_destroy(b); b = NULL;
    equals(particular, 1, 0, "5"); equals(basis, 1, 1, "-2");
}
static void test_overdetermined_unique_and_inconsistent(void)
{
    const char *left[] = {"0", "2", "3", "4", "3", "6"}, *right[] = {"7", "15", "22"};
    a = matrix(3, 2, left); b = matrix(3, 1, right); analyze(); metadata(NUMFORGE_LINEAR_UNIQUE, 2, 0);
    equals(numforge_linear_solution_particular(solution), 0, 0, "1/3");
    equals(numforge_linear_solution_particular(solution), 1, 0, "7/2");
    TEST_ASSERT_NULL(numforge_linear_solution_nullspace(solution));
    numforge_linear_solution_destroy(solution); solution = NULL; numforge_matrix_destroy(b); b = NULL;
    const char *bad[] = {"7", "15", "23"}; b = matrix(3, 1, bad); analyze(); metadata(NUMFORGE_LINEAR_NONE, 2, 0);
    TEST_ASSERT_NULL(numforge_linear_solution_particular(solution)); TEST_ASSERT_NULL(numforge_linear_solution_nullspace(solution));
}
static void test_inconsistent_rank_deficient_still_has_kernel(void)
{
    const char *left[] = {"1/2", "1/3", "3/2", "1"}, *right[] = {"1", "4"};
    a = matrix(2, 2, left); b = matrix(2, 1, right); analyze(); metadata(NUMFORGE_LINEAR_NONE, 1, 1);
    TEST_ASSERT_NULL(numforge_linear_solution_particular(solution));
    equals(numforge_linear_solution_nullspace(solution), 0, 0, "-2/3");
    equals(numforge_linear_solution_nullspace(solution), 1, 0, "1");
}
static void test_zero_rank_and_identity_kernel(void)
{
    const char *left[] = {"0", "0", "0", "0", "0", "0"}, *right[] = {"0", "0", "0"};
    a = matrix(3, 2, left); b = matrix(3, 1, right); analyze(); metadata(NUMFORGE_LINEAR_INFINITE, 0, 2);
    const NumForgeMatrix *basis = numforge_linear_solution_nullspace(solution);
    for (size_t i = 0; i < 2; ++i) {
        TEST_ASSERT_EQUAL_UINT(i, numforge_linear_solution_free_column(solution, i));
        equals(numforge_linear_solution_particular(solution), i, 0, "0");
        for (size_t j = 0; j < 2; ++j) equals(basis, i, j, i == j ? "1" : "0");
    }
    numforge_linear_solution_destroy(solution); solution = NULL; numforge_matrix_destroy(b); b = NULL;
    const char *bad[] = {"0", "0", "1"}; b = matrix(3, 1, bad); analyze(); metadata(NUMFORGE_LINEAR_NONE, 0, 2);
    TEST_ASSERT_NULL(numforge_linear_solution_particular(solution)); equals(numforge_linear_solution_nullspace(solution), 0, 0, "1");
}
static void test_trailing_free_variables_multiple_pivots(void)
{
    const char *left[] = {"1", "2", "3", "4", "0", "0", "2", "6"}, *right[] = {"10", "4"};
    a = matrix(2, 4, left); b = matrix(2, 1, right); analyze(); metadata(NUMFORGE_LINEAR_INFINITE, 2, 2);
    TEST_ASSERT_EQUAL_UINT(0, numforge_linear_solution_pivot_column(solution, 0));
    TEST_ASSERT_EQUAL_UINT(2, numforge_linear_solution_pivot_column(solution, 1));
    TEST_ASSERT_EQUAL_UINT(1, numforge_linear_solution_free_column(solution, 0));
    TEST_ASSERT_EQUAL_UINT(3, numforge_linear_solution_free_column(solution, 1));
    const char *particular[] = {"4", "0", "2", "0"}, *basis[] = {"-2", "5", "1", "0", "0", "-3", "0", "1"};
    for (size_t i = 0; i < 4; ++i) equals(numforge_linear_solution_particular(solution), i, 0, particular[i]);
    for (size_t i = 0; i < 8; ++i) equals(numforge_linear_solution_nullspace(solution), i / 2, i % 2, basis[i]);
}
static void test_thousand_digit_cancellation_and_aliased_inputs(void)
{
    char n[1002], plus[1002], minus[1001];
    n[0] = '1'; memset(n + 1, '0', 1000); n[1001] = '\0'; memcpy(plus, n, sizeof(n)); plus[1000] = '1';
    memset(minus, '9', 1000); minus[1000] = '\0';
    const char *left[] = {n, plus, minus, n}, *right[] = {"-1", "-1"};
    a = matrix(2, 2, left); b = matrix(2, 1, right); analyze(); metadata(NUMFORGE_LINEAR_UNIQUE, 2, 0);
    equals(numforge_linear_solution_particular(solution), 0, 0, "1"); equals(numforge_linear_solution_particular(solution), 1, 0, "-1");
    numforge_matrix_destroy(a); a = NULL; numforge_matrix_destroy(b); b = NULL; numforge_linear_solution_destroy(solution); solution = NULL;
    const char *single[] = {"2/3", "4/3"}; a = matrix(2, 1, single);
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_OK, numforge_matrix_solve_general(&solution, a, a)); metadata(NUMFORGE_LINEAR_UNIQUE, 1, 0);
    equals(numforge_linear_solution_particular(solution), 0, 0, "1");
}
static void test_invalid_inputs_and_null_queries(void)
{
    const char *left[] = {"1", "2"}; a = matrix(1, 2, left); b = matrix(1, 2, left);
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_DIMENSION_MISMATCH, numforge_matrix_solve_general(&solution, a, b));
    numforge_matrix_destroy(b); const char *right[] = {"1", "2"}; b = matrix(2, 1, right);
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_DIMENSION_MISMATCH, numforge_matrix_solve_general(&solution, a, b));
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_NULL_ARGUMENT, numforge_matrix_solve_general(NULL, a, b));
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_NULL_ARGUMENT, numforge_matrix_solve_general(&solution, NULL, b));
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_NULL_ARGUMENT, numforge_matrix_solve_general(&solution, a, NULL));
    size_t rank = 99;
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_NULL_ARGUMENT, numforge_matrix_rank(&rank, NULL)); TEST_ASSERT_EQUAL_UINT(99, rank);
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_NULL_ARGUMENT, numforge_matrix_rank(NULL, a));
    TEST_ASSERT_NULL(solution); TEST_ASSERT_EQUAL(NUMFORGE_LINEAR_INVALID, numforge_linear_solution_kind(NULL));
    TEST_ASSERT_EQUAL_UINT(0, numforge_linear_solution_rank(NULL)); TEST_ASSERT_EQUAL_UINT(0, numforge_linear_solution_nullity(NULL));
    TEST_ASSERT_EQUAL_UINT(SIZE_MAX, numforge_linear_solution_pivot_column(NULL, 0));
    TEST_ASSERT_EQUAL_UINT(SIZE_MAX, numforge_linear_solution_free_column(NULL, 0));
    TEST_ASSERT_NULL(numforge_linear_solution_particular(NULL)); TEST_ASSERT_NULL(numforge_linear_solution_nullspace(NULL));
    numforge_linear_solution_destroy(NULL);
}
static void test_allocation_failures_preserve_owned_destinations(void)
{
    const char *fixtures[][6] = {{"0", "2", "4", "0", "3", "6"}, {"0", "2", "4", "0", "3", "6"},
        {"1", "2", "0", "1", "0", "0"}, {"0", "0", "0", "0", "0", "0"}};
    const char *right[][3] = {{"10", "15", NULL}, {"10", "16", NULL}, {"1", "2", "0"}, {"0", "0", NULL}};
    for (size_t fixture = 0; fixture < 4; ++fixture) {
        size_t rows = fixture == 2 ? 3 : 2, columns = fixture == 2 ? 2 : 3;
        a = matrix(rows, columns, fixtures[fixture]); b = matrix(rows, 1, right[fixture]); analyze();
        for (unsigned operation = 0; operation < 2; ++operation) {
            bool complete = false;
            for (size_t failure = 1; failure < 8192; ++failure) {
                NumForgeLinearSolution *output = solution; size_t rank = 99;
                numforge_test_allocator_begin(failure);
                NumForgeMatrixStatus status = operation ? numforge_matrix_rank(&rank, a) : numforge_matrix_solve_general(&output, a, b);
                bool injected = numforge_test_allocator_did_fail(); numforge_test_allocator_end();
                if (status == NUMFORGE_MATRIX_OK) { if (!operation) numforge_linear_solution_destroy(output); }
                else {
                    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_OUT_OF_MEMORY, status);
                    TEST_ASSERT_EQUAL_PTR(solution, output); TEST_ASSERT_EQUAL_UINT(99, rank);
                }
                equals(a, 0, 1, fixtures[fixture][1]); equals(b, 0, 0, right[fixture][0]);
                if (!injected) { complete = true; break; }
            }
            TEST_ASSERT_TRUE(complete);
        }
        numforge_linear_solution_destroy(solution); solution = NULL;
        numforge_matrix_destroy(a); numforge_matrix_destroy(b); a = b = NULL;
    }
}
static void test_budget_failures_and_scope_ownership(void)
{
    const char *left[] = {"0", "2", "4", "0", "3", "6"}, *right[] = {"10", "15"};
    a = matrix(2, 3, left); b = matrix(2, 1, right); analyze();
    for (unsigned operation = 0; operation < 2; ++operation)
        for (unsigned mode = 0; mode < 4; ++mode) {
            size_t rank = 99; NumForgeLinearSolution *output = solution;
            TEST_ASSERT_TRUE(numforge_budget_begin(5000, mode == 0 ? 0 : SIZE_MAX, mode == 1 ? 0 : SIZE_MAX));
            if (mode >= 2) numforge_test_budget_expire_after(mode == 2 ? 1 : 128);
            NumForgeMatrixStatus status = operation ? numforge_matrix_rank(&rank, a) : numforge_matrix_solve_general(&output, a, b);
            TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_OUT_OF_MEMORY, status);
            TEST_ASSERT_EQUAL_PTR(solution, output); TEST_ASSERT_EQUAL_UINT(99, rank);
            TEST_ASSERT_EQUAL(mode < 2 ? NUMFORGE_BUDGET_MEMORY : NUMFORGE_BUDGET_TIME, numforge_budget_failure());
            TEST_ASSERT_FALSE(numforge_budget_begin(5000, SIZE_MAX, SIZE_MAX)); numforge_budget_end();
        }
    size_t rank = 99; TEST_ASSERT_TRUE(numforge_budget_begin(5000, SIZE_MAX, SIZE_MAX));
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_OK, numforge_matrix_rank(&rank, a)); TEST_ASSERT_EQUAL_UINT(1, rank);
    TEST_ASSERT_FALSE(numforge_budget_begin(5000, SIZE_MAX, SIZE_MAX)); numforge_budget_end();
}
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_skipped_pivots_complete_family_and_owned_lifetime);
    RUN_TEST(test_overdetermined_unique_and_inconsistent);
    RUN_TEST(test_inconsistent_rank_deficient_still_has_kernel);
    RUN_TEST(test_zero_rank_and_identity_kernel);
    RUN_TEST(test_trailing_free_variables_multiple_pivots);
    RUN_TEST(test_thousand_digit_cancellation_and_aliased_inputs);
    RUN_TEST(test_invalid_inputs_and_null_queries);
    RUN_TEST(test_allocation_failures_preserve_owned_destinations);
    RUN_TEST(test_budget_failures_and_scope_ownership);
    return UNITY_END();
}
