#ifndef NUMFORGE_STATISTICS_H
#define NUMFORGE_STATISTICS_H

#include <numforge/bigrational.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @file statistics.h
 * Exact descriptive statistics and linear regression over rational datasets. Independent of
 * expressions, matrices and applications; link the public numeric target.
 * No rounding, precision option or implicit resource budget is used. Existing
 * runtime.h scopes are inherited unchanged; cooperative exhaustion returns
 * BIGRATIONAL_OUT_OF_MEMORY, with budget_failure identifying time/memory cause.
 * Synchronize shared handles externally; independent handles are independent.
 */
typedef struct NumForgeStatisticsSummary NumForgeStatisticsSummary;
typedef struct NumForgeLinearRegression NumForgeLinearRegression;

/** Fit the unique unweighted least-squares line y = slope*x + intercept.
 * @param[out] result New owned immutable fit on success; failures preserve
 * *result and all inputs. Success never frees a previous destination.
 * @param[in] x Borrowed array of count initialized non-NULL real rational scalars.
 * @param[in] y Paired observations, indexed with x. Duplicates, repeated pointers
 * and aliasing between arrays/scalars are allowed. Nothing is modified/retained.
 * @param[in] count At least two pairs, with at least two distinct x values.
 * @return OK, NULL_ARGUMENT for required NULL pointers/entries, INVALID_ARGUMENT
 * for insufficient pairs or constant x, VALUE_TOO_LARGE for storage/numeric limits
 * (pointer-storage overflow is rejected before entries are read), OUT_OF_MEMORY
 * or SCALE_OVERFLOW. Runtime budgets are inherited unchanged.
 *
 * Both coefficients are exact reduced fractions, including noisy data. Exact
 * describes arithmetic on the supplied samples, not measurement accuracy or
 * predictive validity. Constant y gives slope zero and intercept y. No weights,
 * uncertainty estimates, hypothesis tests or rounding/precision option are used.
 * Reuses the anchored exact summary/covariance kernels: linear rational work,
 * input-dependent integer storage. Inputs may be released after success.
 */
BigRationalStatus numforge_statistics_linear_regression(NumForgeLinearRegression **result,
    const BigRational *const *x, const BigRational *const *y, size_t count);
/** Release the fit and both coefficients; NULL accepted. */
void numforge_linear_regression_destroy(NumForgeLinearRegression *fit);
/** Number of paired observations; zero for NULL. */
size_t numforge_linear_regression_count(const NumForgeLinearRegression *fit);
/** Borrow immutable coefficients until fit destruction; NULL for a NULL handle.
 * Never mutate/destroy these scalars or use them as numeric outputs. Copy via
 * bigrational_copy for independent ownership. Synchronize shared handles externally.
 * @{ */
const BigRational *numforge_linear_regression_slope(const NumForgeLinearRegression *fit);
const BigRational *numforge_linear_regression_intercept(const NumForgeLinearRegression *fit);
/** @} */

/** Exact covariance of paired observations left[i], right[i].
 * @param[out] result Initialized owned scalar; may alias any input scalar.
 * Success replaces its value; every failure preserves it and all inputs.
 * @param[in] left Borrowed array of count initialized non-NULL scalars.
 * @param[in] right Borrowed array of count initialized non-NULL scalars.
 * Arrays/scalars may alias and duplicates count separately. Nothing is retained.
 * @param[in] count Positive number of pairs, at least two if sample is true.
 * @param[in] sample Divide sum((x-mean(x))*(y-mean(y))) by count-1 when true,
 * otherwise by count. Population covariance of a singleton is exact zero.
 * @return OK, NULL_ARGUMENT, INVALID_ARGUMENT, OUT_OF_MEMORY, VALUE_TOO_LARGE or
 * SCALE_OVERFLOW. NULL required pointers/entries return NULL_ARGUMENT; invalid
 * count returns INVALID_ARGUMENT. Pointer-storage overflow returns VALUE_TOO_LARGE
 * before entries are read. Inherited cooperative budget failure returns
 * OUT_OF_MEMORY. No precision, rounding or implicit resource scope is used.
 * Differences from each first observation avoid large common-offset products;
 * a fixed number of work scalars does not imply bounded integer storage.
 */
BigRationalStatus numforge_statistics_covariance(BigRational *result,
    const BigRational *const *left, const BigRational *const *right, size_t count, bool sample);

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
