#ifndef NUMFORGE_NUMERICAL_H
#define NUMFORGE_NUMERICAL_H
#include <numforge/bigrational.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/** @file numerical.h
 * Bounded real-root search over caller-supplied functions. Link
 * NumForge::numforge; no parser, engine, session or server is required.
 * Calls inherit an optional runtime.h budget and never begin/end one themselves.
 * Exhaustion returns OUT_OF_MEMORY; inspect numforge_budget_failure() for cause.
 * Independent calls are independent; synchronize shared handles/callback state.
 */
typedef enum NumForgeNumericalStatus {
    NUMFORGE_NUMERICAL_OK = 0,
    NUMFORGE_NUMERICAL_NULL_ARGUMENT,
    NUMFORGE_NUMERICAL_INVALID_ARGUMENT,
    NUMFORGE_NUMERICAL_OUT_OF_MEMORY,
    NUMFORGE_NUMERICAL_VALUE_TOO_LARGE,
    NUMFORGE_NUMERICAL_SCALE_OVERFLOW,
    NUMFORGE_NUMERICAL_CALLBACK_FAILED
} NumForgeNumericalStatus;
typedef enum NumForgeRootKind {
    NUMFORGE_ROOT_FOUND = 0,
    NUMFORGE_ROOT_NOT_BRACKETED,
    NUMFORGE_ROOT_DOMAIN_GAP,
    NUMFORGE_ROOT_DOMAIN_UNVERIFIED,
    NUMFORGE_ROOT_ITERATION_LIMIT,
    NUMFORGE_ROOT_PRECISION_LIMIT
} NumForgeRootKind;
typedef struct NumForgeRoot NumForgeRoot;
typedef struct NumForgeRootSample {
    bool real;       /**< False reports a non-real/undefined sample, not a fatal error. */
    bool exact_zero; /**< True only for a proven mathematical zero, never rounded zero. */
} NumForgeRootSample;
/** Write f(x) into the borrowed destination and set sample.real. x is an exact
 * rational coordinate; decimal_places is the requested working fractional scale,
 * not a guaranteed accuracy. Do not retain/mutate x or destroy value. The solver
 * initializes sample to false/false before every call. Non-real samples need not
 * write value. exact_zero requires real=true and a zero decimal value.
 * Callbacks must be deterministic at a given precision and use no shared mutable
 * policy. Return a non-OK status for fatal failures; no callback resources are
 * owned by the solver. Successful callbacks must respect the requested accuracy.
 */
typedef NumForgeNumericalStatus (*NumForgeRootEvaluate)(BigDecimal *value,
    NumForgeRootSample *sample, const BigRational *x, int64_t decimal_places, void *user);
/** Supply evidence that f is real and continuous on the entire closed interval.
 * Set verified=false if unknown. NULL is allowed and means unverified. This is
 * caller-supplied mathematical evidence, not a certificate inferred by the solver.
 * Exact sampled zeros do not require continuity. Bounds are borrowed read-only.
 */
typedef NumForgeNumericalStatus (*NumForgeRootDomain)(bool *verified,
    const BigRational *lower, const BigRational *upper, void *user);
typedef struct NumForgeRootOptions {
    int64_t decimal_places; /**< Working scale, 1..INT64_MAX/2; defaults to 34. */
    size_t max_iterations;  /**< Positive; defaults to 96. */
    size_t max_evaluations; /**< At least 4; includes verification; defaults to 192. */
    const BigRational *x_tolerance; /**< Borrowed positive absolute width; NULL=1E-12. */
    const BigDecimal *residual_tolerance; /**< Borrowed positive absolute |f(x)|; NULL=1E-12. */
} NumForgeRootOptions;
/** Initialize options; NULL is accepted. */
void numforge_root_options_init(NumForgeRootOptions *options);
/** Search one bracket using exact rational bisection coordinates. Bounds must
 * satisfy lower<upper; arguments/options/tolerances are borrowed for this call.
 * options=NULL selects defaults. Exact endpoint/midpoint zeros are accepted;
 * otherwise opposite endpoint signs are required. Rounded zeros trigger extra
 * quarter probes rather than being labelled exact.
 *
 * Approximate acceptance requires width<=x_tolerance, |f(x)|<=residual_tolerance,
 * strict residual improvement over both initial endpoints, verified continuity,
 * and a fresh sample at twice decimal_places which passes residual/improvement
 * and sign-stability checks. These checks are not a certified root error bound.
 * Missing a bracket, a domain gap or nonconvergence does not prove no roots exist.
 * A small interval alone never establishes convergence; multiple roots are not
 * enumerated. Near multiple/ill-conditioned roots, callers may need more precision.
 *
 * OK returns a newly owned diagnostic result, including non-FOUND outcomes.
 * Failures preserve *result and all inputs; any preexisting *result is not freed.
 * Callback side effects cannot be rolled back. Counts are bounded by the supplied
 * limits; up to three slots are reserved for quarter probes and verification.
 * @return NULL_ARGUMENT for missing mandatory pointers; INVALID_ARGUMENT for
 * invalid bounds/options/tolerances or contradictory exact-zero flags;
 * OUT_OF_MEMORY for allocation/budget exhaustion; VALUE_TOO_LARGE or SCALE_OVERFLOW
 * for representation limits. Non-OK callback statuses propagate unchanged unless
 * inherited budget exhaustion takes precedence,
 * including CALLBACK_FAILED for a caller-specific error retained in user state.
 */
NumForgeNumericalStatus numforge_root_solve(NumForgeRoot **result,
    NumForgeRootEvaluate evaluate, NumForgeRootDomain domain, void *user,
    const BigRational *lower, const BigRational *upper, const NumForgeRootOptions *options);
/** Release owned fields and result; NULL is accepted. */
void numforge_root_destroy(NumForgeRoot *result);
/** Borrowed values valid until destruction. x/residual are NULL unless FOUND.
 * lower/upper describe the final search interval, not a certified enclosure.
 * NULL handles return NULL values, zero counts, false exactness and ITERATION_LIMIT.
 * exact_zero reports the callback's exact-zero proof, not a general exactness flag.
 */
NumForgeRootKind numforge_root_kind(const NumForgeRoot *result);
const BigRational *numforge_root_x(const NumForgeRoot *result);
const BigRational *numforge_root_lower(const NumForgeRoot *result);
const BigRational *numforge_root_upper(const NumForgeRoot *result);
const BigDecimal *numforge_root_residual(const NumForgeRoot *result);
size_t numforge_root_evaluations(const NumForgeRoot *result);
size_t numforge_root_iterations(const NumForgeRoot *result);
bool numforge_root_exact_zero(const NumForgeRoot *result);

/** @name Bounded multiple-root scanning
 * A finite grid with midpoint/quadratic candidate samples, followed by the same
 * guarded single-root solver. This is never a complete root enumeration.
 * @{ */
typedef struct NumForgeRootScan NumForgeRootScan;
typedef enum NumForgeRootScanStop {
    NUMFORGE_SCAN_FINISHED = 0, /**< Grid processed; further roots may exist. */
    NUMFORGE_SCAN_ROOT_LIMIT,
    NUMFORGE_SCAN_EVALUATION_LIMIT
} NumForgeRootScanStop;
/** Same value/flags/precision/ownership contract as NumForgeRootEvaluate.
 * When proposal_value is non-NULL and sample.real=true, also write the original
 * ordinate f(x) as a rational. Prefer its exact value; if only an approximate
 * decimal is available, convert that decimal to a rational. This advisory value
 * proposes extra coordinates and never proves a root. exact_zero additionally
 * requires a zero proposal when supplied. proposal_value=NULL during bracket
 * refinement and higher-precision verification; no rational output is then needed.
 * Every invocation counts as one evaluation, including verification. Do not
 * retain or destroy any borrowed input/output; user state remains caller-owned.
 */
typedef NumForgeNumericalStatus (*NumForgeRootScanEvaluate)(BigDecimal *value,
    BigRational *proposal_value, NumForgeRootSample *sample,
    const BigRational *x, int64_t decimal_places, void *user);
typedef struct NumForgeRootScanOptions {
    NumForgeRootOptions root_options; /**< Per-bracket policy; initialized defaults. */
    size_t segments;        /**< Positive, less than SIZE_MAX; default 128. */
    size_t max_roots;       /**< Positive capacity, default 32. */
    size_t max_evaluations; /**< Positive total callback limit, default 4096. */
} NumForgeRootScanOptions;
typedef struct NumForgeRootScanStats {
    size_t evaluations, iterations, scanned_segments;
    size_t domain_gaps, rejected_brackets;
    size_t candidate_samples, candidate_roots, candidate_gaps;
} NumForgeRootScanStats;
/** Initialize policy; NULL is accepted. Width/residual tolerances retain their
 * independent single-root defaults. Raising grid density does not prove completeness.
 */
void numforge_root_scan_options_init(NumForgeRootScanOptions *options);
/** Scan lower<upper with exact rational coordinates including both endpoints.
 * options=NULL uses defaults. Bounds, callbacks, state and tolerances are borrowed.
 * Roots are sorted by coordinate and only exactly equal coordinates are deduplicated;
 * there is no tolerance merge. Exact sampled zeros are retained, including contacts.
 * Approximate roots require the single-root continuity/residual/stability safeguards.
 * Unseen contacts, several roots within a cell, or domain gaps may be missed.
 * Even the zero function yields only bounded samples, never all of its roots.
 *
 * OK returns a newly owned result, including partial lists stopped by explicit
 * root/evaluation limits. A full per-bracket evaluation allowance is reserved
 * before refinement; unused slots may remain when the total limit stops a scan.
 * Allocation, callback or inherited budget failure preserves *result and returns
 * no partial list. Status meanings and callback-side-effect caveats match
 * numforge_root_solve().
 * Root-array capacity overflow returns VALUE_TOO_LARGE; invalid positive
 * limits, bounds or root options/tolerances return INVALID_ARGUMENT.
 */
NumForgeNumericalStatus numforge_roots_scan(NumForgeRootScan **result,
    NumForgeRootScanEvaluate evaluate, NumForgeRootDomain domain, void *user,
    const BigRational *lower, const BigRational *upper, const NumForgeRootScanOptions *options);
/** Destroy the scan and every contained root; NULL is accepted. */
void numforge_root_scan_destroy(NumForgeRootScan *result);
/** Count (0 for NULL), borrowed root (NULL for invalid index/NULL scan), and
 * stop reason (EVALUATION_LIMIT for NULL). Never destroy a borrowed root yourself;
 * it lives until scan destruction. Query its fields with the single-root getters.
 */
size_t numforge_root_scan_count(const NumForgeRootScan *result);
const NumForgeRoot *numforge_root_scan_root(const NumForgeRootScan *result, size_t index);
NumForgeRootScanStop numforge_root_scan_stop(const NumForgeRootScan *result);
/** Copy work diagnostics without allocation; NULL scan writes zeroes. NULL stats
 * is accepted. evaluations includes all grid/candidate/refinement/verification
 * samples. domain_gaps counts grid edges with a non-real endpoint; candidate_gaps
 * counts non-real extra samples. rejected_brackets counts non-FOUND refinements;
 * candidate_roots counts newly retained exact extra samples, not every refined root.
 */
void numforge_root_scan_stats(const NumForgeRootScan *result, NumForgeRootScanStats *stats);
/** @} */
#ifdef __cplusplus
}
#endif
#endif
