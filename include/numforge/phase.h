#ifndef NUMFORGE_PHASE_H
#define NUMFORGE_PHASE_H
#include <numforge/numerical.h>
#ifdef __cplusplus
extern "C" {
#endif
/** @file phase.h
 * Bounded location of sampled periodic-phase anchors in NumForge::numforge.
 * No parser, session, trigonometric function or rendering dependency is required.
 * Anchors are not certificates of extrema of an arbitrary composed function.
 */
typedef struct NumForgePhaseLocator NumForgePhaseLocator;
typedef struct NumForgePhaseOptions {
    const BigDecimal *period;    /**< Mandatory positive phase-lattice spacing. */
    const BigDecimal *tolerance; /**< Positive absolute phase residual; NULL=1E-6. */
    int64_t significant_digits;  /**< Positive proposal precision, default 34; not callback accuracy. */
    size_t max_evaluations;      /**< Positive callback limit, default 8. */
    size_t reserve_bytes;        /**< Optional inherited allocation reserve, default 0. */
    uint64_t reserve_ms;         /**< Optional inherited time reserve, default 0. */
} NumForgePhaseOptions;
typedef enum NumForgePhaseKind {
    NUMFORGE_PHASE_FOUND = 0,
    NUMFORGE_PHASE_NO_CANDIDATE,  /**< No suitable first candidate; not an exhaustive search. */
    NUMFORGE_PHASE_UNUSABLE,      /**< Callback reported an undefined/unsupported phase. */
    NUMFORGE_PHASE_EVALUATION_LIMIT,
    NUMFORGE_PHASE_RESOURCE_LIMIT,/**< Configured reserve stopped optional work. */
    NUMFORGE_PHASE_PRECISION_LIMIT/**< Rounded proposal/target could not make interior progress. */
} NumForgePhaseKind;
typedef struct NumForgePhaseReport {
    NumForgePhaseKind kind;
    size_t evaluations;
} NumForgePhaseReport;
/** Write the captured phase at the exact borrowed rational coordinate into value
 * and set usable=true. usable starts false; unusable samples need not write value.
 * Do not retain/mutate x or destroy value. Evaluation accuracy and original-function
 * validation belong to the caller. Callback state/outputs remain caller-owned.
 * Non-OK statuses are fatal and propagate unless budget exhaustion takes precedence.
 */
typedef NumForgeNumericalStatus (*NumForgePhaseEvaluate)(BigDecimal *value,
    bool *usable, const BigRational *x, void *user);
/** Set defaults, including period=NULL; NULL options is accepted. */
void numforge_phase_options_init(NumForgePhaseOptions *options);
/** Copy options, period and tolerance into an owned reusable, thread-confined
 * locator. Separate handles may run independently. Failures preserve *result;
 * a preexisting handle is not freed. NULL_ARGUMENT means missing result/options/
 * period; INVALID_ARGUMENT means invalid precision/limit/period/tolerance.
 * Other resource/representation statuses follow numerical.h; budgets are inherited.
 */
NumForgeNumericalStatus numforge_phase_locator_create(NumForgePhaseLocator **result,
    const NumForgePhaseOptions *options);
/** NULL is accepted. */
void numforge_phase_locator_destroy(NumForgePhaseLocator *locator);
/** Search between exact coordinates lower<upper using supplied captured endpoint
 * phases. Consider only the first lattice target offset+k*period strictly above
 * the smaller phase, in increasing phase order even when phase decreases with x.
 * offset=NULL means zero. Equal phases, an out-of-range target or a target within
 * tolerance of either endpoint produce NO_CANDIDATE, not proof of absence.
 * This is not enumeration of all crossings; use bounded, suitably small intervals.
 *
 * Proposals interpolate the current phase bracket at significant_digits precision
 * using half-even rounding, then convert the finite ratio to an exact rational x.
 * Non-interior proposals stop with PRECISION_LIMIT. Each usable original callback
 * sample either meets the absolute phase tolerance or tightens that bracket.
 * Monotonicity, continuity, phase-evaluation accuracy and exactness of a supplied
 * period (e.g. an approximation to pi) are not established by this algorithm.
 * FOUND is a sampled phase match, not a certified root/extremum or error bound in x.
 *
 * OK overwrites *result, including diagnostic outcomes. Fatal errors preserve the
 * report but cannot roll back callback effects. Every call invalidates the previous
 * borrowed locator views, even on error. A FOUND call exposes its last evaluated x
 * and target through getters; copy them before another call if they are needed.
 * The caller can retain its last callback's richer original-function sample without
 * reevaluating the accepted point. No callback is made for NO_CANDIDATE.
 *
 * Check inherited budgets before/after callbacks; no scope is started/ended.
 * Reserves are admission heuristics, not a guarantee the next callback fits;
 * callbacks must cooperate with cancellation. Exhaustion is fatal OUT_OF_MEMORY;
 * runtime.h distinguishes its cause. NULL_ARGUMENT means missing mandatory pointers
 * (user may be NULL); INVALID_ARGUMENT means unordered coordinate bounds. Other
 * representation/resource/callback statuses follow numerical.h.
 */
NumForgeNumericalStatus numforge_phase_locate(NumForgePhaseReport *result,
    NumForgePhaseLocator *locator, const BigRational *lower, const BigRational *upper,
    const BigDecimal *phase_lower, const BigDecimal *phase_upper, const BigDecimal *offset,
    NumForgePhaseEvaluate evaluate, void *user);
/** Borrowed read-only views after FOUND only; otherwise NULL, including NULL handle. */
const BigRational *numforge_phase_locator_x(const NumForgePhaseLocator *locator);
const BigDecimal *numforge_phase_locator_target(const NumForgePhaseLocator *locator);
#ifdef __cplusplus
}
#endif
#endif
