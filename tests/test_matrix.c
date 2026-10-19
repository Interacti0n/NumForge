#include <numforge/matrix.h>
#include <numforge/runtime.h>
#include "src/runtime/numforge_alloc.h"
#include <unity.h>
#include <stdlib.h>
#include <string.h>

static NumForgeMatrix *a, *b, *output;
static BigRational *scalar;
void setUp(void) { scalar = bigrational_create(); TEST_ASSERT_NOT_NULL(scalar); }
void tearDown(void)
{
    numforge_test_allocator_end(); numforge_test_budget_expire_after(0); numforge_budget_end();
    numforge_matrix_destroy(a); numforge_matrix_destroy(b); numforge_matrix_destroy(output);
    a = b = output = NULL; bigrational_destroy(scalar); scalar = NULL;
}
static BigRational *value(const char *text)
{
    char buffer[2048]; const char *slash = strchr(text, '/');
    BigInt *top = bigint_create(), *bottom = bigint_create(); BigRational *v = bigrational_create();
    TEST_ASSERT_NOT_NULL(top); TEST_ASSERT_NOT_NULL(bottom); TEST_ASSERT_NOT_NULL(v);
    size_t length = slash ? (size_t)(slash - text) : strlen(text);
    TEST_ASSERT_TRUE(length < sizeof(buffer)); memcpy(buffer, text, length); buffer[length] = '\0';
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(top, buffer));
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(bottom, slash ? slash + 1 : "1"));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_set_fraction(v, top, bottom));
    bigint_destroy(top); bigint_destroy(bottom); return v;
}
static NumForgeMatrix *matrix(size_t rows, size_t columns, const char *const *texts)
{
    BigRational *values[16] = {0}; const BigRational *entries[16]; NumForgeMatrix *m = NULL;
    TEST_ASSERT_TRUE(rows * columns <= 16);
    for (size_t i = 0; i < rows * columns; ++i) entries[i] = values[i] = value(texts[i]);
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_OK, numforge_matrix_create(&m, rows, columns, entries));
    for (size_t i = 0; i < rows * columns; ++i) bigrational_destroy(values[i]);
    return m;
}
static void equals(const BigRational *v, const char *text)
{
    char *actual = NULL; TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_to_string(v, &actual));
    TEST_ASSERT_EQUAL_STRING(text, actual); free(actual);
}
static void test_ownership_shape_rectangular_product(void)
{
    const char *left[] = {"1", "2", "3", "4", "5", "6"};
    const char *right[] = {"7", "8", "9", "10", "11", "12"};
    a = matrix(2, 3, left); b = matrix(3, 2, right);
    TEST_ASSERT_EQUAL_UINT(2, numforge_matrix_rows(a)); TEST_ASSERT_EQUAL_UINT(3, numforge_matrix_columns(a));
    TEST_ASSERT_NULL(numforge_matrix_entry(a, 2, 0)); TEST_ASSERT_NULL(numforge_matrix_entry(a, 0, SIZE_MAX));
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_OK, numforge_matrix_multiply(&output, a, b));
    equals(numforge_matrix_entry(output, 0, 0), "58"); equals(numforge_matrix_entry(output, 0, 1), "64");
    equals(numforge_matrix_entry(output, 1, 0), "139"); equals(numforge_matrix_entry(output, 1, 1), "154");
    equals(numforge_matrix_entry(a, 0, 0), "1");
    numforge_matrix_destroy(a); a = NULL; numforge_matrix_destroy(b); b = NULL;
    equals(numforge_matrix_entry(output, 1, 1), "154");
}
static void test_rational_pivoting_multiple_rhs(void)
{
    const char *left[] = {"0", "2", "3", "4"}, *rhs[] = {"7", "-4", "15", "7"};
    a = matrix(2, 2, left); b = matrix(2, 2, rhs);
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_OK, numforge_matrix_determinant(scalar, a)); equals(scalar, "-6");
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_OK, numforge_matrix_solve(&output, a, b));
    equals(numforge_matrix_entry(output, 0, 0), "1/3"); equals(numforge_matrix_entry(output, 1, 0), "7/2");
    equals(numforge_matrix_entry(output, 0, 1), "5"); equals(numforge_matrix_entry(output, 1, 1), "-2");
    NumForgeMatrix *product = NULL;
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_OK, numforge_matrix_multiply(&product, a, output));
    for (size_t i = 0; i < 4; ++i) equals(numforge_matrix_entry(product, i / 2, i % 2), rhs[i]);
    numforge_matrix_destroy(product);
}
static void test_fractional_determinant_and_singleton(void)
{
    const char *texts[] = {"1/2", "1/3", "1/4", "1/5"}; a = matrix(2, 2, texts);
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_OK, numforge_matrix_determinant(scalar, a)); equals(scalar, "1/60");
    numforge_matrix_destroy(a); a = NULL;
    const char *single[] = {"-2/3"}, *rhs[] = {"5/7"}; a = matrix(1, 1, single); b = matrix(1, 1, rhs);
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_OK, numforge_matrix_determinant(scalar, a)); equals(scalar, "-2/3");
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_OK, numforge_matrix_solve(&output, a, b)); equals(numforge_matrix_entry(output, 0, 0), "-15/14");
}
static void test_singular_and_repeated_pivots(void)
{
    const char *texts[] = {"1", "2", "2", "4"}, *rhs[] = {"3", "6"};
    a = matrix(2, 2, texts); b = matrix(2, 1, rhs);
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_OK, numforge_matrix_determinant(scalar, a)); equals(scalar, "0");
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_SINGULAR, numforge_matrix_solve(&output, a, b)); TEST_ASSERT_NULL(output);
    numforge_matrix_destroy(b); b = NULL;
    const char *inconsistent[] = {"3", "7"}; b = matrix(2, 1, inconsistent);
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_SINGULAR, numforge_matrix_solve(&output, a, b)); TEST_ASSERT_NULL(output);
    numforge_matrix_destroy(a); a = NULL; numforge_matrix_destroy(b); b = NULL;
    const char *permutation[] = {"0", "0", "1", "1", "0", "0", "0", "1", "0"};
    a = matrix(3, 3, permutation);
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_OK, numforge_matrix_determinant(scalar, a)); equals(scalar, "1");
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_OK, numforge_matrix_solve(&output, a, a));
    for (size_t i = 0; i < 3; ++i)
        for (size_t j = 0; j < 3; ++j) equals(numforge_matrix_entry(output, i, j), i == j ? "1" : "0");
}
static void test_large_exact_cancellation(void)
{
    char n[1002], plus[1002], minus[1001];
    n[0] = '1'; memset(n + 1, '0', 1000); n[1001] = '\0';
    memcpy(plus, n, sizeof(n)); plus[1000] = '1'; memset(minus, '9', 1000); minus[1000] = '\0';
    const char *texts[] = {n, plus, minus, n}, *rhs[] = {"-1", "-1"};
    a = matrix(2, 2, texts); b = matrix(2, 1, rhs);
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_OK, numforge_matrix_determinant(scalar, a)); equals(scalar, "1");
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_OK, numforge_matrix_solve(&output, a, b));
    equals(numforge_matrix_entry(output, 0, 0), "1"); equals(numforge_matrix_entry(output, 1, 0), "-1");
}
static void test_invalid_shapes_nulls_and_aliasing(void)
{
    const char *texts[] = {"1", "2"}; a = matrix(1, 2, texts); b = matrix(1, 2, texts);
    const BigRational *entries[] = {scalar, NULL};
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_INVALID_ARGUMENT, numforge_matrix_create(&output, 0, 1, entries));
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_VALUE_TOO_LARGE, numforge_matrix_create(&output, SIZE_MAX, 2, entries));
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_VALUE_TOO_LARGE, numforge_matrix_create(&output, 1, SIZE_MAX, entries));
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_NULL_ARGUMENT, numforge_matrix_create(&output, 1, 2, entries));
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_NULL_ARGUMENT, numforge_matrix_create(NULL, 1, 1, entries));
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_NULL_ARGUMENT, numforge_matrix_create(&output, 1, 1, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_DIMENSION_MISMATCH, numforge_matrix_multiply(&output, a, b));
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_DIMENSION_MISMATCH, numforge_matrix_determinant(scalar, a)); equals(scalar, "0");
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_DIMENSION_MISMATCH, numforge_matrix_solve(&output, a, b));
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_NULL_ARGUMENT, numforge_matrix_multiply(NULL, a, b));
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_NULL_ARGUMENT, numforge_matrix_multiply(&output, NULL, b));
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_NULL_ARGUMENT, numforge_matrix_determinant(NULL, a));
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_NULL_ARGUMENT, numforge_matrix_determinant(scalar, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_NULL_ARGUMENT, numforge_matrix_solve(&output, a, NULL));
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_NULL_ARGUMENT, numforge_matrix_solve(NULL, a, b));
    TEST_ASSERT_NULL(output); TEST_ASSERT_EQUAL_UINT(0, numforge_matrix_rows(NULL));
    TEST_ASSERT_EQUAL_UINT(0, numforge_matrix_columns(NULL)); TEST_ASSERT_NULL(numforge_matrix_entry(NULL, 0, 0));
    numforge_matrix_destroy(NULL);
    numforge_matrix_destroy(a); a = NULL;
    const BigRational *repeated[] = {numforge_matrix_entry(b, 0, 0), numforge_matrix_entry(b, 0, 0)};
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_OK, numforge_matrix_create(&a, 2, 1, repeated));
    NumForgeMatrix *old = a;
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_OK, numforge_matrix_multiply(&a, a, b));
    numforge_matrix_destroy(old); equals(numforge_matrix_entry(a, 1, 1), "2");
    TEST_ASSERT_EQUAL_STRING("unknown matrix status", numforge_matrix_status_to_string((NumForgeMatrixStatus)999));
}
static NumForgeMatrixStatus operation(unsigned op, NumForgeMatrix **destination, BigRational *det)
{
    const BigRational *entries[] = {numforge_matrix_entry(a, 0, 0), numforge_matrix_entry(a, 0, 1),
        numforge_matrix_entry(a, 1, 0), numforge_matrix_entry(a, 1, 1)};
    if (op == 0) return numforge_matrix_create(destination, 2, 2, entries);
    if (op == 1) return numforge_matrix_multiply(destination, a, b);
    if (op == 2) return numforge_matrix_determinant(det, a);
    return numforge_matrix_solve(destination, a, b);
}
static void test_allocation_failure_preserves_inputs_and_outputs(void)
{
    const char *texts[] = {"0", "2", "3", "4"}, *rhs[] = {"1", "2", "3", "4"};
    a = matrix(2, 2, texts); b = matrix(2, 2, rhs);
    BigRational *sentinel = value("123/7");
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_copy(scalar, sentinel)); bigrational_destroy(sentinel);
    for (unsigned op = 0; op < 4; ++op) {
        bool finished = false;
        for (size_t failure = 1; failure < 8192; ++failure) {
            NumForgeMatrix *destination = b;
            numforge_test_allocator_begin(failure);
            NumForgeMatrixStatus status = operation(op, &destination, scalar);
            bool injected = numforge_test_allocator_did_fail(); numforge_test_allocator_end();
            if (status == NUMFORGE_MATRIX_OK) {
                if (op != 2) numforge_matrix_destroy(destination);
            } else {
                TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_OUT_OF_MEMORY, status);
                TEST_ASSERT_EQUAL_PTR(b, destination); equals(scalar, "123/7");
            }
            equals(numforge_matrix_entry(a, 0, 1), "2"); equals(numforge_matrix_entry(b, 1, 0), "3");
            if (!injected) { finished = true; break; }
        }
        TEST_ASSERT_TRUE(finished);
        sentinel = value("123/7"); TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_copy(scalar, sentinel)); bigrational_destroy(sentinel);
    }
}
static void test_cooperative_budgets_inherit_and_preserve(void)
{
    const char *texts[] = {"0", "2", "3", "4"}; a = matrix(2, 2, texts); b = matrix(2, 2, texts);
    for (unsigned op = 0; op < 4; ++op) {
        for (unsigned mode = 0; mode < 3; ++mode) {
            NumForgeMatrix *destination = b;
            TEST_ASSERT_TRUE(numforge_budget_begin(5000, mode == 0 ? 0 : SIZE_MAX, SIZE_MAX));
            if (mode != 0) numforge_test_budget_expire_after(mode == 1 ? 1 : 16);
            TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_OUT_OF_MEMORY, operation(op, &destination, scalar));
            TEST_ASSERT_EQUAL_PTR(b, destination);
            TEST_ASSERT_EQUAL(mode == 0 ? NUMFORGE_BUDGET_MEMORY : NUMFORGE_BUDGET_TIME, numforge_budget_failure());
            TEST_ASSERT_FALSE(numforge_budget_begin(5000, SIZE_MAX, SIZE_MAX));
            numforge_budget_end(); equals(scalar, "0");
        }
    }
    TEST_ASSERT_TRUE(numforge_budget_begin(5000, SIZE_MAX, SIZE_MAX));
    TEST_ASSERT_EQUAL(NUMFORGE_MATRIX_OK, numforge_matrix_multiply(&output, a, a));
    TEST_ASSERT_FALSE(numforge_budget_begin(5000, SIZE_MAX, SIZE_MAX)); numforge_budget_end();
}
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ownership_shape_rectangular_product);
    RUN_TEST(test_rational_pivoting_multiple_rhs);
    RUN_TEST(test_fractional_determinant_and_singleton);
    RUN_TEST(test_singular_and_repeated_pivots);
    RUN_TEST(test_large_exact_cancellation);
    RUN_TEST(test_invalid_shapes_nulls_and_aliasing);
    RUN_TEST(test_allocation_failure_preserves_inputs_and_outputs);
    RUN_TEST(test_cooperative_budgets_inherit_and_preserve);
    return UNITY_END();
}
