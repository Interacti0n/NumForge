#ifndef NUMFORGE_STATISTICS_H
#define NUMFORGE_STATISTICS_H

#include <numforge/bigrational.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @file statistics.h
 * Exact descriptive statistics over finite rational datasets. Independent of
 * expressions, matrices and applications; link the public numeric target.
 * No rounding, precision option or implicit resource budget is used. Existing
 * runtime.h scopes are inherited unchanged; cooperative exhaustion returns
 * BIGRATIONAL_OUT_OF_MEMORY, with budget_failure identifying time/memory cause.
 * Synchronize shared handles externally; independent handles are independent.
 */
typedef struct NumForgeStatisticsSummary NumForgeStatisticsSummary;

/** Summarize count borrowed rational samples, including duplicates.
 * @param[out] result New owned immutable summary only on success. Every failure
 * preserves *result; success never destroys a previous destination.
 * @param[in] values Array of count non-NULL initialized BigRational pointers.
 * Values may alias; they are never modified or retained after the call.
 * @param[in] count Must be positive (INVALID_ARGUMENT otherwise).
 * @return OK, NULL_ARGUMENT, INVALID_ARGUMENT, OUT_OF_MEMORY, VALUE_TOO_LARGE or
 * SCALE_OVERFLOW. Count/pointer-storage overflow is rejected before entries are
 * read. NULL required pointers/entries return NULL_ARGUMENT.
 *
 * All results are exact reduced fractions: sum, min, max, arithmetic mean and
 * population variance (sum((x-mean)^2)/count). Sample variance uses count-1 and
 * is undefined for a singleton, represented by a NULL accessor, not by zero.
 * Population variance of a singleton is exact zero. Differences from the first
 * sample are accumulated before squaring to avoid large common-offset squares;
 * all cancellation remains exact. Input magnitudes/denominator growth can make
 * arithmetic expensive. No probabilistic inference or approximate error bound
 * is implied. Summary fields outlive every input sample and the input array.
 */
BigRationalStatus numforge_statistics_describe(NumForgeStatisticsSummary **result,
    const BigRational *const *values, size_t count);
/** Release all owned fields; NULL accepted. */
void numforge_statistics_summary_destroy(NumForgeStatisticsSummary *summary);
/** Number of samples; zero for NULL. */
size_t numforge_statistics_summary_count(const NumForgeStatisticsSummary *summary);
/** Borrow immutable fields until summary destruction; NULL for a NULL handle.
 * Never mutate/destroy these scalars or use them as numeric outputs. Copy via
 * bigrational_copy to obtain an independent owned scalar.
 * @{ */
const BigRational *numforge_statistics_summary_sum(const NumForgeStatisticsSummary *summary);
const BigRational *numforge_statistics_summary_minimum(const NumForgeStatisticsSummary *summary);
const BigRational *numforge_statistics_summary_maximum(const NumForgeStatisticsSummary *summary);
const BigRational *numforge_statistics_summary_mean(const NumForgeStatisticsSummary *summary);
const BigRational *numforge_statistics_summary_variance_population(const NumForgeStatisticsSummary *summary);
/** NULL for a singleton, because sample variance requires at least two samples. */
const BigRational *numforge_statistics_summary_variance_sample(const NumForgeStatisticsSummary *summary);
/** @} */

#ifdef __cplusplus
}
#endif
#endif
