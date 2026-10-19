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

/** Exact coefficient rank of any positive rectangular matrix.
 * result is assigned only on success; NULL arguments return NULL_ARGUMENT.
 * Uses rational row elimination, with no rounding or numerical tolerance.
 */
NumForgeMatrixStatus numforge_matrix_rank(size_t *result, const NumForgeMatrix *matrix);

/** Complete exact solution of a rectangular system with ONE RHS column. */
typedef struct NumForgeLinearSolution NumForgeLinearSolution;
typedef enum NumForgeLinearSolutionKind {
    NUMFORGE_LINEAR_INVALID = 0,
    NUMFORGE_LINEAR_NONE,
    NUMFORGE_LINEAR_UNIQUE,
    NUMFORGE_LINEAR_INFINITE
} NumForgeLinearSolutionKind;

/** Analyze a*x=b over the rationals, including under/overdetermined systems.
 * b must have a.rows rows and exactly one column (DIMENSION_MISMATCH otherwise).
 * NULL required arguments return NULL_ARGUMENT. All mathematical outcomes return
 * OK with an owned solution: NONE (inconsistent), UNIQUE, or INFINITE.
 * No least-squares approximation is implied. Existing solve() keeps its square,
 * multi-RHS and SINGULAR contracts unchanged.
 *
 * Success never frees a prior *result; every failure preserves it. Inputs may
 * alias and are copied, never retained or modified. The immutable result owns
 * rank metadata, a canonical particular solution for consistent systems, and
 * a basis of ker(a) whenever nullity>0 (even for inconsistent systems).
 * For a consistent system every solution is particular + nullspace*t, where t
 * is an arbitrary nullity-by-1 rational vector. Pivot/free columns are ascending;
 * particular free coordinates are zero and the nullspace free rows form identity.
 * Uses shared rational elimination/back substitution. No implicit budget is
 * started; resource/numeric failures use NumForgeMatrixStatus as above.
 */
NumForgeMatrixStatus numforge_matrix_solve_general(NumForgeLinearSolution **result,
    const NumForgeMatrix *a, const NumForgeMatrix *b);
/** Destroy an owned analysis, accepting NULL. */
void numforge_linear_solution_destroy(NumForgeLinearSolution *solution);
/** Queries return INVALID/zero for NULL. Nullity is a.columns-rank(a), including
 * inconsistent systems; it describes ker(a), not the existence of solutions. */
NumForgeLinearSolutionKind numforge_linear_solution_kind(const NumForgeLinearSolution *solution);
size_t numforge_linear_solution_rank(const NumForgeLinearSolution *solution);
size_t numforge_linear_solution_nullity(const NumForgeLinearSolution *solution);
/** Zero-based coefficient column indices, SIZE_MAX for NULL/out-of-range. */
size_t numforge_linear_solution_pivot_column(const NumForgeLinearSolution *solution, size_t index);
size_t numforge_linear_solution_free_column(const NumForgeLinearSolution *solution, size_t index);
/** Borrowed immutable matrices, valid until solution destruction. Never destroy
 * them or mutate their entries. particular is a.columns-by-1 for UNIQUE/INFINITE,
 * NULL for NONE/NULL. nullspace is a.columns-by-nullity when nullity>0, NULL
 * otherwise (zero-column matrices remain unsupported). Neither borrows inputs.
 */
const NumForgeMatrix *numforge_linear_solution_particular(const NumForgeLinearSolution *solution);
const NumForgeMatrix *numforge_linear_solution_nullspace(const NumForgeLinearSolution *solution);
/** Static English description; unknown values return "unknown matrix status". */
const char *numforge_matrix_status_to_string(NumForgeMatrixStatus status);

#ifdef __cplusplus
}
#endif
#endif
