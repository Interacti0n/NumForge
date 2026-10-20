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
 * Rational grids, interval scheduling, bounded root search and integration. Link
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

/** Visit one borrowed, read-only exact rational coordinate. index is zero-based.
 * Do not retain x after returning or mutate the grid bounds during the walk.
 * user may be NULL. The callback owns its outputs and any evaluation/rounding
 * policy; coordinate exactness says nothing about function-value accuracy.
 * Return non-OK to stop; callback side effects cannot be rolled back.
 */
typedef NumForgeNumericalStatus (*NumForgeGridVisit)(const BigRational *x,
    size_t index, void *user);
/** Visit count equally spaced points, including both endpoints, in order:
 * x[i] = lower + i*(upper-lower)/(count-1). Requires lower<upper and count>=2.
 * Every coordinate is exact, including nonterminating decimal fractions and
 * offsets too small for binary floating point. No decimal projection is made.
 * Uses constant auxiliary object count; does not retain a point array or call
 * the expression engine. The caller chooses count and owns all sampled values.
 *
 * Checks an inherited budget before and after each callback; never starts or
 * ends a budget. Callbacks must cooperate with it; execution is not preempted.
 * Failure may follow earlier successful visits. Bounds are always preserved.
 * @return NULL_ARGUMENT for missing bounds/callback; INVALID_ARGUMENT for invalid
 * bounds/count; OUT_OF_MEMORY for allocation/budget failure; VALUE_TOO_LARGE or
 * SCALE_OVERFLOW for representation limits. Callback failures propagate unchanged,
 * unless budget exhaustion takes precedence. OK means all count visits completed,
 * not that samples establish continuity, completeness or function-value accuracy.
 */
NumForgeNumericalStatus numforge_grid_visit(const BigRational *lower,
    const BigRational *upper, size_t count, NumForgeGridVisit visit, void *user);

typedef enum NumForgeRefinementDecision {
    NUMFORGE_REFINEMENT_SPLIT = 0, /**< Queue both child intervals for later probes. */
    NUMFORGE_REFINEMENT_ACCEPT,    /**< Callback accepts both child edges. */
    NUMFORGE_REFINEMENT_SKIP       /**< Leave both child edges unresolved; do not revisit. */
} NumForgeRefinementDecision;
/** Probe the current interval [left,left+1] in the caller's ordered point array.
 * On OK, insert exactly one interior point at left+1, shifting the old right
 * endpoint and all later points one place. The scheduler tracks these shifts.
 * Do not insert/delete other points. depth is zero for initial intervals.
 * The caller owns coordinates, values, classifications and edge connections;
 * it must have capacity for options.max_points. The scheduler computes no values.
 * decision starts as SPLIT. ACCEPT/SKIP stop work on both children, but neither
 * certifies continuity, function-value accuracy or complete feature discovery.
 * On failure the callback may have side effects; no rollback is attempted.
 */
typedef NumForgeNumericalStatus (*NumForgeRefinementProbe)(NumForgeRefinementDecision *decision,
    size_t left, size_t depth, void *user);
typedef struct NumForgeRefinementOptions {
    size_t max_points;          /**< At least initial_points; default 2048. */
    size_t max_depth;           /**< Probes require depth<max_depth; default 12. */
    size_t reserve_bytes;       /**< Retained inherited allocation budget; default 0. */
    uint64_t reserve_ms;        /**< Retained inherited time budget; default 0. */
} NumForgeRefinementOptions;
typedef enum NumForgeRefinementLimit {
    NUMFORGE_REFINEMENT_POINT_LIMIT = 1,
    NUMFORGE_REFINEMENT_DEPTH_LIMIT = 2,
    NUMFORGE_REFINEMENT_RESOURCE_LIMIT = 4
} NumForgeRefinementLimit;
typedef struct NumForgeRefinementStats {
    size_t points;              /**< initial_points + successful probes. */
    size_t probes;              /**< Completed callback visits. */
    size_t unresolved_intervals;/**< Pending/depth-limited intervals, excluding callback SKIP. */
    unsigned limits;            /**< OR of NumForgeRefinementLimit; zero if no scheduler limit stopped work. */
} NumForgeRefinementStats;
/** Initialize defaults; NULL is accepted. */
void numforge_refinement_options_init(NumForgeRefinementOptions *options);
/** Refine an existing ordered grid breadth-first, initially left to right.
 * initial_points>=2; options=NULL selects defaults. Options are copied at entry.
 * No expression engine, application session or coordinate representation is
 * required. Allocate the
 * caller's point storage before calling; this function allocates its own queue
 * with O(max_points) space and updates pending indices after every insertion.
 *
 * A point/depth/resource stop returns OK with limits and unresolved counts.
 * Exhausting a depth skips that interval and continues other pending work.
 * Resource admission leaves the configured reserves plus twice the largest
 * observed callback cost (requested bytes/elapsed milliseconds). This predicts
 * work heuristically; it cannot guarantee that the next callback will fit.
 * Callbacks must cooperate with inherited budgets. Exhaustion is a fatal
 * OUT_OF_MEMORY; inspect runtime.h for the cause. No budget is begun or ended.
 *
 * Successful calls overwrite *result. Fatal errors preserve *result, but cannot
 * undo callback effects; discard caller-owned partial data if atomicity is needed.
 * @return NULL_ARGUMENT for missing result/probe; INVALID_ARGUMENT for invalid
 * counts, queue-size overflow or decision; OUT_OF_MEMORY for queue allocation or
 * inherited budget failure. Non-OK callback statuses propagate unchanged unless
 * inherited budget exhaustion takes precedence. No callback is made on initial
 * validation/queue-allocation failure. user may be NULL.
 */
NumForgeNumericalStatus numforge_refine_intervals(NumForgeRefinementStats *result,
    size_t initial_points, const NumForgeRefinementOptions *options,
    NumForgeRefinementProbe probe, void *user);

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

/** @name Finite-interval numerical integration
 * Composite Simpson quadrature on successively halved uniform grids. Coordinates
 * and accumulation of the finite callback decimals are rational, without binary
 * floating point or intermediate rounding. Smooth deterministic integrands are
 * required for the error heuristic to be useful; no domain analysis is performed.
 * @{ */
typedef struct NumForgeIntegral NumForgeIntegral;
typedef enum NumForgeIntegralKind {
    NUMFORGE_INTEGRAL_ESTIMATED = 0, /**< Successive-rule error <= absolute tolerance. */
    NUMFORGE_INTEGRAL_EVALUATION_LIMIT,
    NUMFORGE_INTEGRAL_REFINEMENT_LIMIT,
    NUMFORGE_INTEGRAL_DOMAIN_GAP
} NumForgeIntegralKind;
typedef struct NumForgeIntegralOptions {
    int64_t decimal_places; /**< Callback working precision, positive; default 34. */
    size_t max_refinements; /**< Grid halvings, at least 2; default 12. */
    size_t max_evaluations; /**< At least 5; default 4097. */
    const BigRational *absolute_tolerance; /**< Borrowed positive value; NULL=1E-12. */
} NumForgeIntegralOptions;
/** Initialize defaults; NULL is accepted. */
void numforge_integral_options_init(NumForgeIntegralOptions *options);
/** Integrate over finite exact bounds lower<upper, including endpoint samples.
 * Uses NumForgeRootEvaluate's borrowed decimal output, real flag, cooperative
 * budget and callback-error contract. exact_zero flags are validated but never
 * establish an exact integral. Undefined samples stop with DOMAIN_GAP and no value.
 * User state, options, bounds and tolerance remain borrowed and unmodified.
 *
 * With T_n the composite trapezoid rule, S_2n=(4*T_2n-T_n)/3. The returned value
 * is the latest S_2n, with estimated error |S_2n-S_n|/15. ESTIMATED means only
 * that this heuristic meets the absolute tolerance, never certified accuracy.
 * It excludes callback error; missed oscillations/discontinuities or rounded
 * samples can give zero estimated error for an incorrect answer. Values are
 * approximate even when their rational representation or error is exactly zero.
 * Increasing decimal_places does not guarantee that many correct digits.
 *
 * Each coordinate is sampled once. A complete next grid must fit the evaluation
 * allowance; otherwise return EVALUATION_LIMIT with the previous value/error.
 * REFINEMENT_LIMIT likewise retains the latest estimate. At least two halvings
 * (five evaluations) precede any estimate. O(1) auxiliary numeric objects; integer
 * storage depends on operand sizes. Callbacks must cooperate with inherited
 * runtime budgets; this function neither opens nor closes a scope.
 *
 * OK creates an owned diagnostic result, including limit/domain outcomes.
 * Fatal failures preserve *result; success never frees its previous value.
 * NULL_ARGUMENT for missing mandatory pointers, INVALID_ARGUMENT for invalid
 * bounds/options/tolerance or inconsistent sample flags; allocation/budget and
 * representation errors follow numforge_root_solve(). Callback statuses propagate,
 * with exhausted inherited budgets taking precedence. Side effects are not undone.
 */
NumForgeNumericalStatus numforge_integrate(NumForgeIntegral **result,
    NumForgeRootEvaluate evaluate, void *user, const BigRational *lower,
    const BigRational *upper, const NumForgeIntegralOptions *options);
/** Destroy owned result; NULL is accepted. */
void numforge_integral_destroy(NumForgeIntegral *result);
/** Borrowed value/error, NULL for DOMAIN_GAP or a NULL handle; never destroy them.
 * NULL handles report EVALUATION_LIMIT and zero counts. Refinements count completed
 * grid halvings; evaluations include a final non-real sample when present.
 */
const BigRational *numforge_integral_value(const NumForgeIntegral *result);
const BigRational *numforge_integral_error(const NumForgeIntegral *result);
NumForgeIntegralKind numforge_integral_kind(const NumForgeIntegral *result);
size_t numforge_integral_evaluations(const NumForgeIntegral *result);
size_t numforge_integral_refinements(const NumForgeIntegral *result);
/** @} */

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
