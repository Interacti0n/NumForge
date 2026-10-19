#include <numforge/matrix.h>
#include <numforge/runtime.h>
#include <stdint.h>
#include <stdlib.h>

struct NumForgeMatrix {
    size_t rows, columns;
    BigRational **entries;
};
struct NumForgeLinearSolution {
    NumForgeLinearSolutionKind kind;
    size_t rank, nullity;
    size_t *pivots, *free_columns;
    NumForgeMatrix *particular, *nullspace;
};
#define CELL(m, r, c) ((m)->entries[(r) * (m)->columns + (c)])

static NumForgeMatrixStatus numeric(BigRationalStatus status)
{
    switch (status) {
        case BIGRATIONAL_OK: return NUMFORGE_MATRIX_OK;
        case BIGRATIONAL_NULL_ARGUMENT: return NUMFORGE_MATRIX_NULL_ARGUMENT;
        case BIGRATIONAL_OUT_OF_MEMORY: return NUMFORGE_MATRIX_OUT_OF_MEMORY;
        case BIGRATIONAL_VALUE_TOO_LARGE: return NUMFORGE_MATRIX_VALUE_TOO_LARGE;
        case BIGRATIONAL_SCALE_OVERFLOW: return NUMFORGE_MATRIX_SCALE_OVERFLOW;
        default: return NUMFORGE_MATRIX_INVALID_ARGUMENT;
    }
}
static NumForgeMatrixStatus checked(NumForgeMatrixStatus status)
{ return numforge_budget_check() ? status : NUMFORGE_MATRIX_OUT_OF_MEMORY; }

void numforge_matrix_destroy(NumForgeMatrix *matrix)
{
    if (matrix == NULL) return;
    if (matrix->entries != NULL)
        for (size_t i = 0; i < matrix->rows * matrix->columns; ++i)
            bigrational_destroy(matrix->entries[i]);
    free(matrix->entries);
    free(matrix);
}
static NumForgeMatrixStatus allocate(NumForgeMatrix **result, size_t rows, size_t columns)
{
    if (rows == 0 || columns == 0) return NUMFORGE_MATRIX_INVALID_ARGUMENT;
    if (rows > SIZE_MAX / columns || rows * columns > SIZE_MAX / sizeof(BigRational *))
        return NUMFORGE_MATRIX_VALUE_TOO_LARGE;
    NumForgeMatrix *matrix = numforge_calloc(1, sizeof(*matrix));
    if (matrix == NULL) return NUMFORGE_MATRIX_OUT_OF_MEMORY;
    matrix->rows = rows; matrix->columns = columns;
    matrix->entries = numforge_calloc(rows * columns, sizeof(*matrix->entries));
    NumForgeMatrixStatus status = matrix->entries ? NUMFORGE_MATRIX_OK : NUMFORGE_MATRIX_OUT_OF_MEMORY;
    for (size_t i = 0; status == NUMFORGE_MATRIX_OK && i < rows * columns; ++i) {
        matrix->entries[i] = bigrational_create();
        if (matrix->entries[i] == NULL) status = NUMFORGE_MATRIX_OUT_OF_MEMORY;
    }
    status = checked(status);
    if (status == NUMFORGE_MATRIX_OK) *result = matrix;
    else numforge_matrix_destroy(matrix);
    return status;
}
NumForgeMatrixStatus numforge_matrix_create(NumForgeMatrix **result, size_t rows,
    size_t columns, const BigRational *const *entries)
{
    if (result == NULL || entries == NULL) return NUMFORGE_MATRIX_NULL_ARGUMENT;
    NumForgeMatrix *matrix = NULL;
    NumForgeMatrixStatus status = allocate(&matrix, rows, columns);
    for (size_t i = 0; status == NUMFORGE_MATRIX_OK && i < rows * columns; ++i)
        status = entries[i] == NULL ? NUMFORGE_MATRIX_NULL_ARGUMENT : numeric(bigrational_copy(matrix->entries[i], entries[i]));
    status = checked(status);
    if (status == NUMFORGE_MATRIX_OK) *result = matrix;
    else numforge_matrix_destroy(matrix);
    return status;
}
static NumForgeMatrixStatus copy(NumForgeMatrix **result, const NumForgeMatrix *source)
{
    NumForgeMatrix *matrix = NULL;
    NumForgeMatrixStatus status = allocate(&matrix, source->rows, source->columns);
    for (size_t i = 0; status == NUMFORGE_MATRIX_OK && i < source->rows * source->columns; ++i)
        status = numeric(bigrational_copy(matrix->entries[i], source->entries[i]));
    if (status == NUMFORGE_MATRIX_OK) *result = matrix;
    else numforge_matrix_destroy(matrix);
    return status;
}
size_t numforge_matrix_rows(const NumForgeMatrix *matrix) { return matrix ? matrix->rows : 0; }
size_t numforge_matrix_columns(const NumForgeMatrix *matrix) { return matrix ? matrix->columns : 0; }
const BigRational *numforge_matrix_entry(const NumForgeMatrix *matrix, size_t row, size_t column)
{ return matrix && row < matrix->rows && column < matrix->columns ? CELL(matrix, row, column) : NULL; }

NumForgeMatrixStatus numforge_matrix_multiply(NumForgeMatrix **result, const NumForgeMatrix *a, const NumForgeMatrix *b)
{
    if (result == NULL || a == NULL || b == NULL) return NUMFORGE_MATRIX_NULL_ARGUMENT;
    if (a->columns != b->rows) return NUMFORGE_MATRIX_DIMENSION_MISMATCH;
    NumForgeMatrix *product = NULL;
    NumForgeMatrixStatus status = allocate(&product, a->rows, b->columns);
    BigRational *term = NULL;
    if (status == NUMFORGE_MATRIX_OK && (term = bigrational_create()) == NULL) status = NUMFORGE_MATRIX_OUT_OF_MEMORY;
    for (size_t i = 0; status == NUMFORGE_MATRIX_OK && i < a->rows; ++i)
        for (size_t j = 0; status == NUMFORGE_MATRIX_OK && j < b->columns; ++j)
            for (size_t k = 0; status == NUMFORGE_MATRIX_OK && k < a->columns; ++k) {
                status = checked(numeric(bigrational_mul(term, CELL(a, i, k), CELL(b, k, j))));
                if (status == NUMFORGE_MATRIX_OK)
                    status = numeric(bigrational_add(CELL(product, i, j), CELL(product, i, j), term));
            }
    status = checked(status);
    bigrational_destroy(term);
    if (status == NUMFORGE_MATRIX_OK) *result = product;
    else numforge_matrix_destroy(product);
    return status;
}
static void exchange_rows(NumForgeMatrix *matrix, size_t a, size_t b)
{
    for (size_t j = 0; j < matrix->columns; ++j) {
        BigRational *temporary = CELL(matrix, a, j);
        CELL(matrix, a, j) = CELL(matrix, b, j); CELL(matrix, b, j) = temporary;
    }
}
/* Scratch matrices are private, so a failed row update is never observable. */
static NumForgeMatrixStatus eliminate(NumForgeMatrix *a, NumForgeMatrix *b,
    BigRational *determinant, size_t *rank, size_t *pivots)
{
    BigRational *zero = bigrational_create(), *factor = bigrational_create(), *term = bigrational_create();
    NumForgeMatrixStatus status = zero && factor && term ? NUMFORGE_MATRIX_OK : NUMFORGE_MATRIX_OUT_OF_MEMORY;
    *rank = 0;
    for (size_t k = 0; status == NUMFORGE_MATRIX_OK && k < a->columns && *rank < a->rows; ++k) {
        size_t row = *rank, pivot = row;
        for (; pivot < a->rows; ++pivot) {
            int comparison = 0;
            status = checked(numeric(bigrational_compare(&comparison, CELL(a, pivot, k), zero)));
            if (status != NUMFORGE_MATRIX_OK || comparison != 0) break;
        }
        if (status != NUMFORGE_MATRIX_OK) break;
        if (pivot == a->rows) continue;
        if (pivot != row) {
            exchange_rows(a, row, pivot);
            if (b) exchange_rows(b, row, pivot);
            if (determinant) status = numeric(bigrational_negate(determinant, determinant));
        }
        if (determinant && status == NUMFORGE_MATRIX_OK)
            status = numeric(bigrational_mul(determinant, determinant, CELL(a, row, k)));
        if (pivots) pivots[row] = k;
        ++*rank;
        for (size_t i = row + 1; status == NUMFORGE_MATRIX_OK && i < a->rows; ++i) {
            status = numeric(bigrational_div(factor, CELL(a, i, k), CELL(a, row, k)));
            for (size_t j = k + 1; status == NUMFORGE_MATRIX_OK && j < a->columns; ++j) {
                status = checked(numeric(bigrational_mul(term, factor, CELL(a, row, j))));
                if (status == NUMFORGE_MATRIX_OK) status = numeric(bigrational_sub(CELL(a, i, j), CELL(a, i, j), term));
            }
            if (status == NUMFORGE_MATRIX_OK) status = numeric(bigrational_copy(CELL(a, i, k), zero));
            for (size_t j = 0; b && status == NUMFORGE_MATRIX_OK && j < b->columns; ++j) {
                status = checked(numeric(bigrational_mul(term, factor, CELL(b, row, j))));
                if (status == NUMFORGE_MATRIX_OK) status = numeric(bigrational_sub(CELL(b, i, j), CELL(b, i, j), term));
            }
        }
    }
    if (status == NUMFORGE_MATRIX_OK && determinant && *rank < a->rows)
        status = numeric(bigrational_copy(determinant, zero));
    bigrational_destroy(zero); bigrational_destroy(factor); bigrational_destroy(term);
    return checked(status);
}
NumForgeMatrixStatus numforge_matrix_determinant(BigRational *result, const NumForgeMatrix *matrix)
{
    if (result == NULL || matrix == NULL) return NUMFORGE_MATRIX_NULL_ARGUMENT;
    if (matrix->rows != matrix->columns) return NUMFORGE_MATRIX_DIMENSION_MISMATCH;
    NumForgeMatrix *a = NULL;
    NumForgeMatrixStatus status = copy(&a, matrix);
    BigRational *det = bigrational_create(); BigInt *one = bigint_create(); size_t rank = 0;
    if (det == NULL || one == NULL) status = NUMFORGE_MATRIX_OUT_OF_MEMORY;
    if (status == NUMFORGE_MATRIX_OK) {
        BigIntStatus parsed = bigint_set_string(one, "1");
        if (parsed != BIGINT_OK) status = parsed == BIGINT_OUT_OF_MEMORY ? NUMFORGE_MATRIX_OUT_OF_MEMORY : NUMFORGE_MATRIX_INVALID_ARGUMENT;
    }
    if (status == NUMFORGE_MATRIX_OK) status = numeric(bigrational_from_bigint(det, one));
    if (status == NUMFORGE_MATRIX_OK) status = eliminate(a, NULL, det, &rank, NULL);
    status = checked(status);
    if (status == NUMFORGE_MATRIX_OK) status = numeric(bigrational_copy(result, det));
    bigrational_destroy(det); bigint_destroy(one); numforge_matrix_destroy(a);
    return status;
}
/* Free coordinates in output are initialized by the caller. right==NULL is
 * homogeneous back substitution, shared by all nullspace columns. */
static NumForgeMatrixStatus substitute(NumForgeMatrix *output, const NumForgeMatrix *left,
    const NumForgeMatrix *right, size_t rank, const size_t *pivots)
{
    BigRational *term = bigrational_create();
    NumForgeMatrixStatus status = term ? NUMFORGE_MATRIX_OK : NUMFORGE_MATRIX_OUT_OF_MEMORY;
    for (size_t row = rank; status == NUMFORGE_MATRIX_OK && row > 0;) {
        --row;
        size_t pivot = pivots ? pivots[row] : row;
        for (size_t c = 0; status == NUMFORGE_MATRIX_OK && c < output->columns; ++c) {
            if (right) status = numeric(bigrational_copy(CELL(output, pivot, c), CELL(right, row, c)));
            for (size_t j = pivot + 1; status == NUMFORGE_MATRIX_OK && j < left->columns; ++j) {
                status = checked(numeric(bigrational_mul(term, CELL(left, row, j), CELL(output, j, c))));
                if (status == NUMFORGE_MATRIX_OK) status = numeric(bigrational_sub(CELL(output, pivot, c), CELL(output, pivot, c), term));
            }
            if (status == NUMFORGE_MATRIX_OK) status = numeric(bigrational_div(CELL(output, pivot, c), CELL(output, pivot, c), CELL(left, row, pivot)));
        }
    }
    bigrational_destroy(term); return checked(status);
}
NumForgeMatrixStatus numforge_matrix_solve(NumForgeMatrix **result, const NumForgeMatrix *a, const NumForgeMatrix *b)
{
    if (result == NULL || a == NULL || b == NULL) return NUMFORGE_MATRIX_NULL_ARGUMENT;
    if (a->rows != a->columns || a->rows != b->rows) return NUMFORGE_MATRIX_DIMENSION_MISMATCH;
    NumForgeMatrix *left = NULL, *right = NULL, *solution = NULL; size_t rank = 0;
    NumForgeMatrixStatus status = copy(&left, a);
    if (status == NUMFORGE_MATRIX_OK) status = copy(&right, b);
    if (status == NUMFORGE_MATRIX_OK) status = eliminate(left, right, NULL, &rank, NULL);
    if (status == NUMFORGE_MATRIX_OK && rank < a->rows) status = NUMFORGE_MATRIX_SINGULAR;
    if (status == NUMFORGE_MATRIX_OK) status = allocate(&solution, a->rows, b->columns);
    if (status == NUMFORGE_MATRIX_OK) status = substitute(solution, left, right, rank, NULL);
    status = checked(status);
    if (status == NUMFORGE_MATRIX_OK) *result = solution;
    else numforge_matrix_destroy(solution);
    numforge_matrix_destroy(left); numforge_matrix_destroy(right);
    return status;
}

NumForgeMatrixStatus numforge_matrix_rank(size_t *result, const NumForgeMatrix *matrix)
{
    if (result == NULL || matrix == NULL) return NUMFORGE_MATRIX_NULL_ARGUMENT;
    NumForgeMatrix *work = NULL; size_t rank = 0;
    NumForgeMatrixStatus status = copy(&work, matrix);
    if (status == NUMFORGE_MATRIX_OK) status = eliminate(work, NULL, NULL, &rank, NULL);
    status = checked(status);
    if (status == NUMFORGE_MATRIX_OK) *result = rank;
    numforge_matrix_destroy(work); return status;
}
void numforge_linear_solution_destroy(NumForgeLinearSolution *solution)
{
    if (solution == NULL) return;
    free(solution->pivots); free(solution->free_columns);
    numforge_matrix_destroy(solution->particular); numforge_matrix_destroy(solution->nullspace); free(solution);
}
NumForgeMatrixStatus numforge_matrix_solve_general(NumForgeLinearSolution **result,
    const NumForgeMatrix *a, const NumForgeMatrix *b)
{
    if (result == NULL || a == NULL || b == NULL) return NUMFORGE_MATRIX_NULL_ARGUMENT;
    if (a->rows != b->rows || b->columns != 1) return NUMFORGE_MATRIX_DIMENSION_MISMATCH;
    NumForgeLinearSolution *solution = numforge_calloc(1, sizeof(*solution));
    if (solution == NULL) return NUMFORGE_MATRIX_OUT_OF_MEMORY;
    NumForgeMatrix *left = NULL, *right = NULL; BigRational *zero = NULL; BigInt *one = NULL;
    NumForgeMatrixStatus status = copy(&left, a);
    if (status == NUMFORGE_MATRIX_OK) status = copy(&right, b);
    size_t maximum_rank = a->rows < a->columns ? a->rows : a->columns;
    if (status == NUMFORGE_MATRIX_OK) {
        solution->pivots = numforge_calloc(maximum_rank, sizeof(*solution->pivots));
        if (solution->pivots == NULL) status = NUMFORGE_MATRIX_OUT_OF_MEMORY;
    }
    if (status == NUMFORGE_MATRIX_OK) status = eliminate(left, right, NULL, &solution->rank, solution->pivots);
    if (status == NUMFORGE_MATRIX_OK) {
        solution->nullity = a->columns - solution->rank;
        solution->kind = solution->nullity ? NUMFORGE_LINEAR_INFINITE : NUMFORGE_LINEAR_UNIQUE;
        zero = bigrational_create();
        if (zero == NULL) status = NUMFORGE_MATRIX_OUT_OF_MEMORY;
    }
    for (size_t i = solution->rank; status == NUMFORGE_MATRIX_OK && i < a->rows; ++i) {
        int comparison = 0;
        status = checked(numeric(bigrational_compare(&comparison, CELL(right, i, 0), zero)));
        if (comparison) solution->kind = NUMFORGE_LINEAR_NONE;
    }
    if (status == NUMFORGE_MATRIX_OK && solution->kind != NUMFORGE_LINEAR_NONE) {
        status = allocate(&solution->particular, a->columns, 1);
        if (status == NUMFORGE_MATRIX_OK)
            status = substitute(solution->particular, left, right, solution->rank, solution->pivots);
    }
    if (status == NUMFORGE_MATRIX_OK && solution->nullity) {
        solution->free_columns = numforge_calloc(solution->nullity, sizeof(*solution->free_columns));
        if (solution->free_columns == NULL) status = NUMFORGE_MATRIX_OUT_OF_MEMORY;
        if (status == NUMFORGE_MATRIX_OK) status = allocate(&solution->nullspace, a->columns, solution->nullity);
        one = bigint_create();
        if (one == NULL) status = NUMFORGE_MATRIX_OUT_OF_MEMORY;
        if (status == NUMFORGE_MATRIX_OK && bigint_set_string(one, "1") != BIGINT_OK) status = NUMFORGE_MATRIX_OUT_OF_MEMORY;
        size_t pivot = 0, free_index = 0;
        for (size_t j = 0; status == NUMFORGE_MATRIX_OK && j < a->columns; ++j) {
            if (pivot < solution->rank && solution->pivots[pivot] == j) ++pivot;
            else {
                solution->free_columns[free_index] = j;
                status = numeric(bigrational_from_bigint(CELL(solution->nullspace, j, free_index), one));
                ++free_index;
            }
        }
        if (status == NUMFORGE_MATRIX_OK)
            status = substitute(solution->nullspace, left, NULL, solution->rank, solution->pivots);
    }
    status = checked(status);
    if (status == NUMFORGE_MATRIX_OK) *result = solution;
    else numforge_linear_solution_destroy(solution);
    bigrational_destroy(zero); bigint_destroy(one); numforge_matrix_destroy(left); numforge_matrix_destroy(right);
    return status;
}
NumForgeLinearSolutionKind numforge_linear_solution_kind(const NumForgeLinearSolution *s)
{ return s ? s->kind : NUMFORGE_LINEAR_INVALID; }
size_t numforge_linear_solution_rank(const NumForgeLinearSolution *s) { return s ? s->rank : 0; }
size_t numforge_linear_solution_nullity(const NumForgeLinearSolution *s) { return s ? s->nullity : 0; }
size_t numforge_linear_solution_pivot_column(const NumForgeLinearSolution *s, size_t index)
{ return s && index < s->rank ? s->pivots[index] : SIZE_MAX; }
size_t numforge_linear_solution_free_column(const NumForgeLinearSolution *s, size_t index)
{ return s && index < s->nullity ? s->free_columns[index] : SIZE_MAX; }
const NumForgeMatrix *numforge_linear_solution_particular(const NumForgeLinearSolution *s)
{ return s ? s->particular : NULL; }
const NumForgeMatrix *numforge_linear_solution_nullspace(const NumForgeLinearSolution *s)
{ return s ? s->nullspace : NULL; }
const char *numforge_matrix_status_to_string(NumForgeMatrixStatus status)
{
    switch (status) {
        case NUMFORGE_MATRIX_OK: return "ok";
        case NUMFORGE_MATRIX_NULL_ARGUMENT: return "null argument";
        case NUMFORGE_MATRIX_INVALID_ARGUMENT: return "invalid matrix argument";
        case NUMFORGE_MATRIX_DIMENSION_MISMATCH: return "matrix dimensions do not match";
        case NUMFORGE_MATRIX_SINGULAR: return "singular coefficient matrix";
        case NUMFORGE_MATRIX_OUT_OF_MEMORY: return "out of memory or resource budget exhausted";
        case NUMFORGE_MATRIX_VALUE_TOO_LARGE: return "matrix or numeric value too large";
        case NUMFORGE_MATRIX_SCALE_OVERFLOW: return "numeric scale overflow";
        default: return "unknown matrix status";
    }
}
