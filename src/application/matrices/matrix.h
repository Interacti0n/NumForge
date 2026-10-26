#ifndef NUMFORGE_APPLICATION_MATRIX_H
#define NUMFORGE_APPLICATION_MATRIX_H
#include "src/engine/api/calculator_internal.h"
#include <numforge/matrix.h>

#define APPLICATION_MATRIX_MAX_DIMENSION 8U
#define APPLICATION_MATRIX_MAX_CELL 64U
typedef struct ApplicationMatrixResult {
    NumForgeMatrix *matrix;
    char *scalar;
    NumForgeLinearSolution *solution;
} ApplicationMatrixResult;

/* Stateless exact adapter: rows separated by LF (CRLF accepted), entries by
 * spaces/tabs. Signed integers, finite dot decimals and integer fractions only;
 * no expressions, exponents, variables or sessions. Up to 8x8, 64 bytes/cell.
 * operations: determinant, rank, multiply, solve (square A, multi-column B),
 * solve_general (rectangular A, single-column B; none/unique/infinite).
 * Optional B is ignored for unary operations. All work inherits/starts the
 * supplied budget. Success publishes a new owned result; every failure leaves
 * *result unchanged. failed_matrix=0/1 and zero-based failed_cell identify parse
 * errors, SIZE_MAX otherwise. Error offset is local to that matrix's text.
 * SINGULAR maps to DIVISION_BY_ZERO; shape errors to INVALID_ARGUMENT.
 * Caller destroys the result; NULL is accepted. No input is retained. */
CalculatorStatus application_matrix_compute(ApplicationMatrixResult **result,
    const char *operation, const char *a, const char *b,
    const CalculatorContext *context, CalculatorError *error,
    size_t *failed_matrix, size_t *failed_cell);
void application_matrix_result_destroy(ApplicationMatrixResult *result);
#endif
