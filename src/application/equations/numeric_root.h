#ifndef NUMFORGE_APPLICATION_NUMERIC_ROOT_H
#define NUMFORGE_APPLICATION_NUMERIC_ROOT_H
#include "src/application/equations/equation.h"

/* Private compatibility names retain the established HTTP/application schema. */
#include "src/modules/numerical/root_internal.h"
typedef NumForgeRootKind ApplicationNumericRootKind;
typedef NumForgeRoot ApplicationNumericRoot;
#define APPLICATION_ROOT_FOUND NUMFORGE_ROOT_FOUND
#define APPLICATION_ROOT_NOT_BRACKETED NUMFORGE_ROOT_NOT_BRACKETED
#define APPLICATION_ROOT_DOMAIN_GAP NUMFORGE_ROOT_DOMAIN_GAP
#define APPLICATION_ROOT_DOMAIN_UNVERIFIED NUMFORGE_ROOT_DOMAIN_UNVERIFIED
#define APPLICATION_ROOT_ITERATION_LIMIT NUMFORGE_ROOT_ITERATION_LIMIT
#define APPLICATION_ROOT_PRECISION_LIMIT NUMFORGE_ROOT_PRECISION_LIMIT

/* One bounded, approximate real root, never an enumeration or certified error bound.
 * x/residual exist only for FOUND; lower/upper retain the final search interval.
 * exact_zero means the sampled typed rational difference is exactly zero.
 * Otherwise both absolute width and absolute residual must be <= 1E-12 and
 * residual must improve strictly on both original endpoints, and both sides
 * must pass conservative real-continuity checks on the final interval.
 * Non-exact candidates are reevaluated at twice the search working precision;
 * disagreement or insufficient residual improvement returns PRECISION_LIMIT.
 * Read-only owned fields, released by application_numeric_root_destroy. */


/* One equality, lowercase x, radians, no session/RNG/assignment. At most
 * 192 point pairs (including final verification) and 96 bisections;
 * one inherited operation budget. Bounds
 * are borrowed rational values with lower<upper; context precision 16..100.
 * Exact rational endpoint zeros are accepted; otherwise opposite endpoint
 * signs are required. Absence of a bracket does not imply absence of roots.
 * Structural/resource failures preserve *result. Numerical nonconvergence and
 * sampled domain gaps are successful diagnostic results without any root. */
CalculatorStatus application_numeric_root_solve(ApplicationNumericRoot **result,
    const char *input, const BigRational *lower, const BigRational *upper,
    const CalculatorContext *context, CalculatorError *error);
void application_numeric_root_destroy(ApplicationNumericRoot *result);
const char *application_numeric_root_kind(ApplicationNumericRootKind kind);

#define APPLICATION_ROOT_SCAN_SEGMENTS 128U
#define APPLICATION_ROOT_SCAN_MAX_ROOTS 32U
#define APPLICATION_ROOT_SCAN_MAX_EVALUATIONS 4096U
typedef enum ApplicationRootScanStop {
    APPLICATION_SCAN_FINISHED, APPLICATION_SCAN_ROOT_LIMIT, APPLICATION_SCAN_EVALUATION_LIMIT
} ApplicationRootScanStop;
/* Rational grid with bounded midpoint/local-quadratic candidates, followed by
 * the production single-bracket solver. Candidate samples use ORIGINAL sides;
 * only typed exact zeros or accepted sign brackets can publish roots.
 * Never a complete enumeration: multiple roots per cell, unsampled tangencies
 * and unresolved domains can be missed. Exact sampled zeros are retained.
 * Ordered owned roots, exact-coordinate duplicates removed (no tolerance merge).
 * Diagnostics count sampled non-real grid edges and rejected sign brackets.
 * A finished grid does not prove absence of further roots. */
typedef struct ApplicationRootScan {
    ApplicationNumericRoot *roots[APPLICATION_ROOT_SCAN_MAX_ROOTS];
    size_t count, evaluations, iterations, scanned_segments, domain_gaps, rejected_brackets;
    size_t candidate_samples, candidate_roots, candidate_gaps; /* candidate_roots counts exact extra samples only */
    ApplicationRootScanStop stop;
} ApplicationRootScan;
/* Same input/context contract and atomic failures as the single-root solver.
 * All sampling, refinements and allocation share one inherited budget. */
CalculatorStatus application_numeric_roots_scan(ApplicationRootScan **result,
    const char *input, const BigRational *lower, const BigRational *upper,
    const CalculatorContext *context, CalculatorError *error);
void application_numeric_roots_destroy(ApplicationRootScan *result);
const char *application_numeric_roots_stop(ApplicationRootScanStop stop);
#endif
