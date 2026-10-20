#include <numforge/statistics.h>
#include <numforge/runtime.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum Field { SUM, MINIMUM, MAXIMUM, MEAN, POPULATION, SAMPLE, FIELD_COUNT };
struct NumForgeStatisticsSummary {
    size_t count;
    BigRational *fields[FIELD_COUNT];
};

void numforge_statistics_summary_destroy(NumForgeStatisticsSummary *summary)
{
    if (summary == NULL) return;
    for (size_t i = 0; i < FIELD_COUNT; ++i) bigrational_destroy(summary->fields[i]);
    free(summary);
}
static BigRationalStatus set_count(BigRational *value, size_t count)
{
    char text[32]; BigInt *integer = bigint_create();
    if (integer == NULL) return BIGRATIONAL_OUT_OF_MEMORY;
    (void)snprintf(text, sizeof(text), "%zu", count);
    BigIntStatus parsed = bigint_set_string(integer, text);
    BigRationalStatus status = parsed == BIGINT_OK ? bigrational_from_bigint(value, integer) :
        parsed == BIGINT_OUT_OF_MEMORY ? BIGRATIONAL_OUT_OF_MEMORY : BIGRATIONAL_INVALID_ARGUMENT;
    bigint_destroy(integer); return status;
}
BigRationalStatus numforge_statistics_covariance(BigRational *result,
    const BigRational *const *left, const BigRational *const *right, size_t count, bool sample)
{
    if (result == NULL || left == NULL || right == NULL) return BIGRATIONAL_NULL_ARGUMENT;
    if (count == 0 || (sample && count < 2)) return BIGRATIONAL_INVALID_ARGUMENT;
    if (count > SIZE_MAX / sizeof(*left)) return BIGRATIONAL_VALUE_TOO_LARGE;
    for (size_t i = 0; i < count; ++i) {
        if (left[i] == NULL || right[i] == NULL) return BIGRATIONAL_NULL_ARGUMENT;
        if (!numforge_budget_check()) return BIGRATIONAL_OUT_OF_MEMORY;
    }
    enum { DX, DY, SX, SY, PRODUCTS, TERM, N, DENOMINATOR, ANSWER, WORK_COUNT };
    BigRational *work[WORK_COUNT] = {0}; BigRationalStatus status = BIGRATIONAL_OK;
    for (size_t i = 0; status == BIGRATIONAL_OK && i < WORK_COUNT; ++i) {
        work[i] = bigrational_create();
        if (work[i] == NULL) status = BIGRATIONAL_OUT_OF_MEMORY;
    }
    for (size_t i = 1; status == BIGRATIONAL_OK && i < count; ++i) {
        status = bigrational_sub(work[DX], left[i], left[0]);
        if (status == BIGRATIONAL_OK) status = bigrational_sub(work[DY], right[i], right[0]);
        if (status == BIGRATIONAL_OK) status = bigrational_add(work[SX], work[SX], work[DX]);
        if (status == BIGRATIONAL_OK) status = bigrational_add(work[SY], work[SY], work[DY]);
        if (status == BIGRATIONAL_OK) status = bigrational_mul(work[TERM], work[DX], work[DY]);
        if (status == BIGRATIONAL_OK) status = bigrational_add(work[PRODUCTS], work[PRODUCTS], work[TERM]);
        if (!numforge_budget_check()) status = BIGRATIONAL_OUT_OF_MEMORY;
    }
    if (status == BIGRATIONAL_OK) status = set_count(work[N], count);
    /* n*sum(dx*dy)-sum(dx)*sum(dy), with no rounded means. */
    if (status == BIGRATIONAL_OK) status = bigrational_mul(work[ANSWER], work[N], work[PRODUCTS]);
    if (status == BIGRATIONAL_OK) status = bigrational_mul(work[TERM], work[SX], work[SY]);
    if (status == BIGRATIONAL_OK) status = bigrational_sub(work[ANSWER], work[ANSWER], work[TERM]);
    if (status == BIGRATIONAL_OK) status = set_count(work[DENOMINATOR], sample ? count - 1 : count);
    if (status == BIGRATIONAL_OK) status = bigrational_mul(work[DENOMINATOR], work[N], work[DENOMINATOR]);
    if (status == BIGRATIONAL_OK) status = bigrational_div(work[ANSWER], work[ANSWER], work[DENOMINATOR]);
    if (!numforge_budget_check()) status = BIGRATIONAL_OUT_OF_MEMORY;
    if (status == BIGRATIONAL_OK) status = bigrational_copy(result, work[ANSWER]);
    for (size_t i = 0; i < WORK_COUNT; ++i) bigrational_destroy(work[i]);
    return status;
}
BigRationalStatus numforge_statistics_describe(NumForgeStatisticsSummary **result,
    const BigRational *const *values, size_t count)
{
    if (result == NULL || values == NULL) return BIGRATIONAL_NULL_ARGUMENT;
    if (count == 0) return BIGRATIONAL_INVALID_ARGUMENT;
    if (count > SIZE_MAX / sizeof(*values)) return BIGRATIONAL_VALUE_TOO_LARGE;
    for (size_t i = 0; i < count; ++i) {
        if (values[i] == NULL) return BIGRATIONAL_NULL_ARGUMENT;
        if (!numforge_budget_check()) return BIGRATIONAL_OUT_OF_MEMORY;
    }
    NumForgeStatisticsSummary *summary = numforge_calloc(1, sizeof(*summary));
    if (summary == NULL) return BIGRATIONAL_OUT_OF_MEMORY;
    summary->count = count;
    enum { DELTA, DELTA_SUM, SQUARE_SUM, TERM, N, DENOMINATOR, NUMERATOR, WORK_COUNT };
    BigRational *work[WORK_COUNT] = {0}; BigRationalStatus status = BIGRATIONAL_OK;
    for (size_t i = 0; status == BIGRATIONAL_OK && i < FIELD_COUNT; ++i) {
        if (i == SAMPLE && count == 1) continue;
        summary->fields[i] = bigrational_create();
        if (summary->fields[i] == NULL) status = BIGRATIONAL_OUT_OF_MEMORY;
    }
    for (size_t i = 0; status == BIGRATIONAL_OK && i < WORK_COUNT; ++i) {
        work[i] = bigrational_create();
        if (work[i] == NULL) status = BIGRATIONAL_OUT_OF_MEMORY;
    }
    if (status == BIGRATIONAL_OK) status = set_count(work[N], count);
    if (status == BIGRATIONAL_OK) status = bigrational_copy(summary->fields[MINIMUM], values[0]);
    if (status == BIGRATIONAL_OK) status = bigrational_copy(summary->fields[MAXIMUM], values[0]);
    for (size_t i = 1; status == BIGRATIONAL_OK && i < count; ++i) {
        status = bigrational_sub(work[DELTA], values[i], values[0]);
        if (status == BIGRATIONAL_OK) status = bigrational_add(work[DELTA_SUM], work[DELTA_SUM], work[DELTA]);
        if (status == BIGRATIONAL_OK) status = bigrational_mul(work[TERM], work[DELTA], work[DELTA]);
        if (status == BIGRATIONAL_OK) status = bigrational_add(work[SQUARE_SUM], work[SQUARE_SUM], work[TERM]);
        int comparison = 0;
        if (status == BIGRATIONAL_OK) status = bigrational_compare(&comparison, values[i], summary->fields[MINIMUM]);
        if (status == BIGRATIONAL_OK && comparison < 0) status = bigrational_copy(summary->fields[MINIMUM], values[i]);
        if (status == BIGRATIONAL_OK) status = bigrational_compare(&comparison, values[i], summary->fields[MAXIMUM]);
        if (status == BIGRATIONAL_OK && comparison > 0) status = bigrational_copy(summary->fields[MAXIMUM], values[i]);
        if (!numforge_budget_check()) status = BIGRATIONAL_OUT_OF_MEMORY;
    }
    if (status == BIGRATIONAL_OK) status = bigrational_div(summary->fields[MEAN], work[DELTA_SUM], work[N]);
    if (status == BIGRATIONAL_OK) status = bigrational_add(summary->fields[MEAN], summary->fields[MEAN], values[0]);
    if (status == BIGRATIONAL_OK) status = bigrational_mul(summary->fields[SUM], summary->fields[MEAN], work[N]);
    /* n*sum(d^2)-sum(d)^2 is exact; no rounded mean enters the variance. */
    if (status == BIGRATIONAL_OK) status = bigrational_mul(work[NUMERATOR], work[N], work[SQUARE_SUM]);
    if (status == BIGRATIONAL_OK) status = bigrational_mul(work[TERM], work[DELTA_SUM], work[DELTA_SUM]);
    if (status == BIGRATIONAL_OK) status = bigrational_sub(work[NUMERATOR], work[NUMERATOR], work[TERM]);
    if (status == BIGRATIONAL_OK) status = bigrational_mul(work[DENOMINATOR], work[N], work[N]);
    if (status == BIGRATIONAL_OK) status = bigrational_div(summary->fields[POPULATION], work[NUMERATOR], work[DENOMINATOR]);
    if (status == BIGRATIONAL_OK && count > 1) {
        status = set_count(work[DENOMINATOR], count - 1);
        if (status == BIGRATIONAL_OK) status = bigrational_mul(work[DENOMINATOR], work[N], work[DENOMINATOR]);
        if (status == BIGRATIONAL_OK) status = bigrational_div(summary->fields[SAMPLE], work[NUMERATOR], work[DENOMINATOR]);
    }
    if (!numforge_budget_check()) status = BIGRATIONAL_OUT_OF_MEMORY;
    if (status == BIGRATIONAL_OK) *result = summary;
    else numforge_statistics_summary_destroy(summary);
    for (size_t i = 0; i < WORK_COUNT; ++i) bigrational_destroy(work[i]);
    return status;
}
size_t numforge_statistics_summary_count(const NumForgeStatisticsSummary *s) { return s ? s->count : 0; }
const BigRational *numforge_statistics_summary_sum(const NumForgeStatisticsSummary *s) { return s ? s->fields[SUM] : NULL; }
const BigRational *numforge_statistics_summary_minimum(const NumForgeStatisticsSummary *s) { return s ? s->fields[MINIMUM] : NULL; }
const BigRational *numforge_statistics_summary_maximum(const NumForgeStatisticsSummary *s) { return s ? s->fields[MAXIMUM] : NULL; }
const BigRational *numforge_statistics_summary_mean(const NumForgeStatisticsSummary *s) { return s ? s->fields[MEAN] : NULL; }
const BigRational *numforge_statistics_summary_variance_population(const NumForgeStatisticsSummary *s) { return s ? s->fields[POPULATION] : NULL; }
const BigRational *numforge_statistics_summary_variance_sample(const NumForgeStatisticsSummary *s) { return s ? s->fields[SAMPLE] : NULL; }
