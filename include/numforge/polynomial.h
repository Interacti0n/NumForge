#ifndef NUMFORGE_POLYNOMIAL_H
#define NUMFORGE_POLYNOMIAL_H
#include <numforge/bigrationalcomplex.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @file polynomial.h
 * @brief Exact solution of a*x^2+b*x+c=0 for real rational coefficients.
 * Link NumForge::numforge; no parser, engine context, session or server required.
 * All computations are exact, including the representation of irrational roots.
 * Operations inherit an optional runtime budget and never start their own scope.
 * Exhaustion returns OUT_OF_MEMORY; query numforge_budget_failure() before ending
 * the scope. Independent handles may be used on separate threads; sharing a handle
 * requires caller synchronization. No decimal approximations are produced here.
 */
typedef struct NumForgePolynomialSolution NumForgePolynomialSolution;
/** NONE/ALL describe constant equations over the complex numbers. FINITE includes
 * real and principal conjugate complex roots. INVALID is returned only for NULL.
 */
typedef enum NumForgePolynomialKind {
    NUMFORGE_POLYNOMIAL_NONE = 0,
    NUMFORGE_POLYNOMIAL_FINITE,
    NUMFORGE_POLYNOMIAL_ALL,
    NUMFORGE_POLYNOMIAL_INVALID
} NumForgePolynomialKind;

/** Solve with borrowed coefficients, including aliases. Zero leading coefficients
 * reduce the degree; the identically zero polynomial returns ALL. Successful output
 * is newly owned; no input or existing destination is freed. Failure preserves
 * *result and all coefficients. No digit cap/rounding is imposed.
 * @return OK, NULL_ARGUMENT, OUT_OF_MEMORY or a BigRational representation error.
 */
BigRationalStatus numforge_polynomial_solve(NumForgePolynomialSolution **result,
    const BigRational *a, const BigRational *b, const BigRational *c);
/** Release a solution; NULL is accepted. */
void numforge_polynomial_destroy(NumForgePolynomialSolution *solution);
/** Query kind (INVALID for NULL), reduced degree (0 for NULL/constant),
 * distinct-root count (0 for NULL/NONE/ALL), or multiplicity (0 for invalid index).
 */
NumForgePolynomialKind numforge_polynomial_kind(const NumForgePolynomialSolution *solution);
unsigned numforge_polynomial_degree(const NumForgePolynomialSolution *solution);
size_t numforge_polynomial_root_count(const NumForgePolynomialSolution *solution);
unsigned numforge_polynomial_root_multiplicity(const NumForgePolynomialSolution *solution, size_t index);
/** Whether roots have nonzero imaginary offsets; false for NULL/NONE/ALL. */
bool numforge_polynomial_is_imaginary(const NumForgePolynomialSolution *solution);
/** @name Borrowed exact root descriptors
 * FINITE roots are center +/- sqrt(radicand), or center +/- i*sqrt(radicand)
 * when is_imaginary() is true. Index 0 uses minus and index 1 plus; a single root
 * is center. radicand >= 0. rational_offset is its nonnegative rational square
 * root, or NULL if irrational. The discriminant is b*b-4*a*c for degree 2 only.
 * Descriptors are read-only and valid until solution destruction. NULL solutions
 * and inapplicable fields return NULL. No square-factor simplification is promised.
 * @{ */
const BigRational *numforge_polynomial_center(const NumForgePolynomialSolution *solution);
const BigRational *numforge_polynomial_radicand(const NumForgePolynomialSolution *solution);
const BigRational *numforge_polynomial_rational_offset(const NumForgePolynomialSolution *solution);
const BigRational *numforge_polynomial_discriminant(const NumForgePolynomialSolution *solution);
/** @} */
/** Materialize a new owned rational-complex root. A real root has imaginary zero.
 * @return OK, NULL_ARGUMENT, OUT_OF_MEMORY, a numeric representation error, or
 * INVALID_ARGUMENT for an out-of-range/non-finite root or an irrational offset.
 * Inspect rational_offset() to distinguish irrational roots: their exact
 * descriptor remains available. Never silently approximates; failure preserves
 * *result. Free a successful value with bigrationalcomplex_destroy().
 */
BigRationalStatus numforge_polynomial_root_exact(BigRationalComplex **result,
    const NumForgePolynomialSolution *solution, size_t index);

#ifdef __cplusplus
}
#endif
#endif
