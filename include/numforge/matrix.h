#ifndef NUMFORGE_MATRIX_H
#define NUMFORGE_MATRIX_H

#include <numforge/bigrational.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @file matrix.h
 * Exact dense rational matrices, independent of expressions and applications.
 * Handles own immutable reduced rational entries. No rounding or implicit
 * resource scope is used. Optional runtime.h budgets are inherited unchanged;
 * cooperative exhaustion returns OUT_OF_MEMORY (query budget_failure for cause).
 * Integer sizes can grow substantially during elimination. This is an initial
 * classical rational implementation, not a sparse/BLAS or approximate backend.
 * Synchronize shared handles externally; independent handles are independent.
 */
typedef struct NumForgeMatrix NumForgeMatrix;

typedef enum NumForgeMatrixStatus {
    NUMFORGE_MATRIX_OK = 0,
    NUMFORGE_MATRIX_NULL_ARGUMENT,
    NUMFORGE_MATRIX_INVALID_ARGUMENT,
    NUMFORGE_MATRIX_DIMENSION_MISMATCH,
    NUMFORGE_MATRIX_SINGULAR,
    NUMFORGE_MATRIX_OUT_OF_MEMORY,
    NUMFORGE_MATRIX_VALUE_TOO_LARGE,
    NUMFORGE_MATRIX_SCALE_OVERFLOW
} NumForgeMatrixStatus;

/** Copy rows*columns non-NULL borrowed values in row-major order.
 * Both dimensions must be positive (INVALID_ARGUMENT). Overflow in dimensions
 * or pointer storage returns VALUE_TOO_LARGE before entries are read.
 * NULL result/entries/entry returns NULL_ARGUMENT. Inputs may alias each other.
 * Every matrix-producing call assigns a new owned handle only on success,
 * preserves *result on every failure, and never destroys a previous destination.
 * An output slot may hold an input handle; retain/free that old handle yourself.
 * All functions may return allocation/resource or numeric size failures.
 */
NumForgeMatrixStatus numforge_matrix_create(NumForgeMatrix **result, size_t rows,
    size_t columns, const BigRational *const *entries);
/** Release all entries; NULL is accepted. */
void numforge_matrix_destroy(NumForgeMatrix *matrix);
/** Shape queries return zero for NULL. */
size_t numforge_matrix_rows(const NumForgeMatrix *matrix);
size_t numforge_matrix_columns(const NumForgeMatrix *matrix);
/** Borrow an immutable entry until destruction; NULL for invalid index/handle.
 * Never mutate/destroy the returned value or use it as a numeric output.
 */
const BigRational *numforge_matrix_entry(const NumForgeMatrix *matrix, size_t row, size_t column);
/** Exact product a*b. a.columns must equal b.rows (DIMENSION_MISMATCH).
 * Aliased inputs (including a==b) are supported; neither is modified.
 */
NumForgeMatrixStatus numforge_matrix_multiply(NumForgeMatrix **result,
    const NumForgeMatrix *a, const NumForgeMatrix *b);
/** Exact determinant by rational Gaussian elimination and row exchanges.
 * Requires a square matrix (DIMENSION_MISMATCH). Singular matrices succeed
 * with exact zero. result is an initialized, independently owned BigRational;
 * its value is preserved on every failure. NULL arguments return NULL_ARGUMENT.
 */
NumForgeMatrixStatus numforge_matrix_determinant(BigRational *result, const NumForgeMatrix *matrix);
/** Solve a*x=b exactly, with one or more right-hand-side columns.
 * a must be square and b.rows==a.rows (DIMENSION_MISMATCH). SINGULAR means
 * a has no inverse: it does not distinguish inconsistent systems from systems
 * with infinitely many solutions. No partial solution is published.
 * Success owns an a.columns by b.columns matrix; a and b may be the same handle.
 * NULL arguments return NULL_ARGUMENT. Uses elimination/back substitution,
 * with exact nonzero pivot selection; no numerical tolerance or precision option.
 */
NumForgeMatrixStatus numforge_matrix_solve(NumForgeMatrix **result,
    const NumForgeMatrix *a, const NumForgeMatrix *b);
/** Static English description; unknown values return "unknown matrix status". */
const char *numforge_matrix_status_to_string(NumForgeMatrixStatus status);

#ifdef __cplusplus
}
#endif
#endif
