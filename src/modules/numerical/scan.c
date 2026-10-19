#include "src/modules/numerical/root_internal.h"
#include "src/modules/numerical/numerical_internal.h"
#include <numforge/runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct Probe { BigDecimal *value; int sign; bool real, exact_zero; BigRational *proposal_value; } Probe;
typedef struct ScanWork {
    NumForgeRootScanEvaluate evaluate;
    NumForgeRootDomain domain;
    void *user;
    const NumForgeRootScanOptions *options;
    size_t evaluations;
    const BigRational *zero;
} ScanWork;
static NumForgeNumericalStatus checked(NumForgeNumericalStatus status)
{ return numforge_budget_check() ? status : NUMFORGE_NUMERICAL_OUT_OF_MEMORY; }
void numforge_root_scan_options_init(NumForgeRootScanOptions *options)
{
    if (!options) return;
    numforge_root_options_init(&options->root_options);
    options->segments = 128; options->max_roots = 32; options->max_evaluations = 4096;
}
void numforge_root_scan_destroy(NumForgeRootScan *result)
{
    if (!result) return;
    for (size_t i = 0; i < result->count; ++i) numforge_root_destroy(result->roots[i]);
    free(result->roots); free(result);
}
static NumForgeNumericalStatus refinement_evaluate(BigDecimal *value, NumForgeRootSample *sample,
    const BigRational *x, int64_t digits, void *user)
{
    ScanWork *work = user;
    return work->evaluate(value, NULL, sample, x, digits, work->user);
}
static NumForgeNumericalStatus refinement_domain(bool *verified,
    const BigRational *lower, const BigRational *upper, void *user)
{
    ScanWork *work = user; return work->domain(verified, lower, upper, work->user);
}
static NumForgeNumericalStatus probe(ScanWork *work, const BigRational *x, Probe *result)
{
    NumForgeRootSample sample = {false, false};
    result->real = false; result->exact_zero = false; result->sign = 0;
    ++work->evaluations;
    NumForgeNumericalStatus status = work->evaluate(result->value, result->proposal_value, &sample,
        x, work->options->root_options.decimal_places, work->user);
    result->real = sample.real; result->exact_zero = sample.exact_zero;
    if (status == NUMFORGE_NUMERICAL_OK && sample.exact_zero && !sample.real) status = NUMFORGE_NUMERICAL_INVALID_ARGUMENT;
    if (status == NUMFORGE_NUMERICAL_OK && sample.real) status = decimal(bigdecimal_sign(&result->sign, result->value));
    if (status == NUMFORGE_NUMERICAL_OK && sample.exact_zero) {
        int sign = 1;
        status = rational(bigrational_compare(&sign, result->proposal_value, work->zero));
        if (status == NUMFORGE_NUMERICAL_OK && (sign || result->sign)) status = NUMFORGE_NUMERICAL_INVALID_ARGUMENT;
    }
    return checked(status);
}
static NumForgeNumericalStatus store_exact(NumForgeRoot *result, const BigRational *x, const Probe *sample)
{
    result->x = bigrational_create(); result->residual = bigdecimal_create();
    NumForgeNumericalStatus status = result->x && result->residual ? NUMFORGE_NUMERICAL_OK : NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_copy(result->x, x));
    if (status == NUMFORGE_NUMERICAL_OK) status = decimal(bigdecimal_abs(result->residual, sample->value));
    if (status == NUMFORGE_NUMERICAL_OK) { result->kind = NUMFORGE_ROOT_FOUND; result->exact_zero = true; }
    return checked(status);
}
static NumForgeNumericalStatus scan_append(NumForgeRootScan *output, NumForgeRoot **candidate)
{
    if (output->count >= output->capacity) { output->stop = NUMFORGE_SCAN_ROOT_LIMIT; return NUMFORGE_NUMERICAL_OK; }
    int order = 1; size_t position = output->count;
    NumForgeNumericalStatus status = NUMFORGE_NUMERICAL_OK;
    while (position && status == NUMFORGE_NUMERICAL_OK) {
        status = rational(bigrational_compare(&order, output->roots[position-1U]->x, (*candidate)->x));
        if (order <= 0) break;
        --position;
    }
    if (status == NUMFORGE_NUMERICAL_OK && order != 0) {
        memmove(output->roots+position+1U, output->roots+position, (output->count-position)*sizeof(output->roots[0]));
        output->roots[position] = *candidate; ++output->count; *candidate = NULL;
        if (output->count == output->capacity) output->stop = NUMFORGE_SCAN_ROOT_LIMIT;
    }
    return status;
}
static NumForgeNumericalStatus scan_exact(NumForgeRootScan *output, const BigRational *x, const Probe *sample)
{
    NumForgeRoot *candidate = numforge_calloc(1U, sizeof(*candidate));
    NumForgeNumericalStatus status = candidate ? NUMFORGE_NUMERICAL_OK : NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    if (status == NUMFORGE_NUMERICAL_OK) {
        candidate->lower = bigrational_create(); candidate->upper = bigrational_create();
        if (!candidate->lower || !candidate->upper) status = NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    }
    if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_copy(candidate->lower, x));
    if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_copy(candidate->upper, x));
    if (status == NUMFORGE_NUMERICAL_OK) status = store_exact(candidate, x, sample);
    if (status == NUMFORGE_NUMERICAL_OK) { candidate->evaluations = 1U; status = scan_append(output, &candidate); }
    numforge_root_destroy(candidate); return status;
}
static NumForgeNumericalStatus scan_bracket(ScanWork *work, NumForgeRootScan *output,
    const BigRational *left, const BigRational *right)
{
    if (output->stop != NUMFORGE_SCAN_FINISHED) return NUMFORGE_NUMERICAL_OK;
    if (work->options->root_options.max_evaluations > work->options->max_evaluations-work->evaluations) { output->stop = NUMFORGE_SCAN_EVALUATION_LIMIT; return NUMFORGE_NUMERICAL_OK; }
    NumForgeRoot *candidate = NULL;
    NumForgeNumericalStatus status = numforge_root_solve(&candidate, refinement_evaluate, work->domain ? refinement_domain : NULL, work, left, right, &work->options->root_options);
    if (status == NUMFORGE_NUMERICAL_OK) {
        work->evaluations += candidate->evaluations; output->iterations += candidate->iterations;
        if (candidate->kind == NUMFORGE_ROOT_FOUND) status = scan_append(output, &candidate);
        else ++output->rejected_brackets;
    }
    numforge_root_destroy(candidate); return status;
}
enum { SC_MID, SC_VERTEX, SC_NUMERATOR, SC_DENOMINATOR, SC_SPAN, SC_TWO, SC_FOUR, SC_ZERO, SC_COUNT };
typedef struct ScanScratch { BigRational *values[SC_COUNT]; Probe middle, vertex; } ScanScratch;
static NumForgeNumericalStatus scan_candidate_probe(ScanWork *work, NumForgeRootScan *output, const BigRational *x, Probe *sample)
{
    if (work->evaluations >= work->options->max_evaluations) { output->stop = NUMFORGE_SCAN_EVALUATION_LIMIT; return NUMFORGE_NUMERICAL_OK; }
    ++output->candidate_samples;
    NumForgeNumericalStatus status = probe(work, x, sample);
    if (status == NUMFORGE_NUMERICAL_OK && !sample->real) ++output->candidate_gaps;
    return status;
}
/* A midpoint and the local interpolating parabola propose extra ORIGINAL
 * equation samples. A parabola minimum is never a root by assumption: only a
 * typed exact zero, or ordinary guarded sign brackets, may publish a result. */
static NumForgeNumericalStatus scan_cell(ScanWork *work, NumForgeRootScan *output, ScanScratch *scratch,
    const BigRational *left, const BigRational *right, const Probe *a, const Probe *b)
{
    if (!a->real || !b->real || !a->sign || !b->sign || a->sign != b->sign) return NUMFORGE_NUMERICAL_OK;
    BigRational **s = scratch->values; Probe *m = &scratch->middle, *v = &scratch->vertex;
    NumForgeNumericalStatus status = midpoint(s[SC_MID], left, right, s[SC_TWO]);
    if (status == NUMFORGE_NUMERICAL_OK) status = scan_candidate_probe(work, output, s[SC_MID], m);
    if (status != NUMFORGE_NUMERICAL_OK || output->stop != NUMFORGE_SCAN_FINISHED || !m->real) return status;
    if (m->exact_zero) { size_t before=output->count; status = scan_exact(output, s[SC_MID], m); if (status == NUMFORGE_NUMERICAL_OK && output->count>before) ++output->candidate_roots; return status; }
    if (m->sign && m->sign != a->sign) {
        status = scan_bracket(work, output, left, s[SC_MID]);
        if (status == NUMFORGE_NUMERICAL_OK) status = scan_bracket(work, output, s[SC_MID], right);
        return status;
    }
    // vertex = midpoint + span*(a-b)/(4*(a-2*m+b)), all rational.
    status = rational(bigrational_mul(s[SC_DENOMINATOR], m->proposal_value, s[SC_TWO]));
    if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_add(s[SC_NUMERATOR], a->proposal_value, b->proposal_value));
    if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_sub(s[SC_DENOMINATOR], s[SC_NUMERATOR], s[SC_DENOMINATOR]));
    int order = 0;
    if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_compare(&order, s[SC_DENOMINATOR], s[SC_ZERO]));
    if (status != NUMFORGE_NUMERICAL_OK || !order) return status;
    status = rational(bigrational_mul(s[SC_DENOMINATOR], s[SC_DENOMINATOR], s[SC_FOUR]));
    if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_sub(s[SC_NUMERATOR], a->proposal_value, b->proposal_value));
    if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_sub(s[SC_SPAN], right, left));
    if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_mul(s[SC_NUMERATOR], s[SC_NUMERATOR], s[SC_SPAN]));
    if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_div(s[SC_VERTEX], s[SC_NUMERATOR], s[SC_DENOMINATOR]));
    if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_add(s[SC_VERTEX], s[SC_VERTEX], s[SC_MID]));
    if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_compare(&order, s[SC_VERTEX], left));
    if (status != NUMFORGE_NUMERICAL_OK || order <= 0) return status;
    status = rational(bigrational_compare(&order, s[SC_VERTEX], right));
    if (status != NUMFORGE_NUMERICAL_OK || order >= 0) return status;
    status = rational(bigrational_compare(&order, s[SC_VERTEX], s[SC_MID]));
    if (status != NUMFORGE_NUMERICAL_OK || !order) return status;
    status = scan_candidate_probe(work, output, s[SC_VERTEX], v);
    if (status != NUMFORGE_NUMERICAL_OK || output->stop != NUMFORGE_SCAN_FINISHED || !v->real) return status;
    if (v->exact_zero) { size_t before=output->count; status = scan_exact(output, s[SC_VERTEX], v); if (status == NUMFORGE_NUMERICAL_OK && output->count>before) ++output->candidate_roots; return status; }
    if (v->sign && v->sign != a->sign) {
        status = scan_bracket(work, output, left, s[SC_VERTEX]);
        if (status == NUMFORGE_NUMERICAL_OK) status = scan_bracket(work, output, s[SC_VERTEX], right);
    }
    return status;
}
NumForgeNumericalStatus numforge_roots_scan(NumForgeRootScan **result,
    NumForgeRootScanEvaluate evaluate, NumForgeRootDomain domain, void *user,
    const BigRational *lower, const BigRational *upper, const NumForgeRootScanOptions *options)
{
    if (!result || !evaluate || !lower || !upper) return NUMFORGE_NUMERICAL_NULL_ARGUMENT;
    NumForgeRootScanOptions defaults; numforge_root_scan_options_init(&defaults);
    if (!options) options = &defaults;
    if (!options->segments || options->segments == SIZE_MAX || !options->max_roots ||
        !options->max_evaluations || !numerical_options_valid(&options->root_options)) return NUMFORGE_NUMERICAL_INVALID_ARGUMENT;
    if (options->max_roots > SIZE_MAX/sizeof(NumForgeRoot *)) return NUMFORGE_NUMERICAL_VALUE_TOO_LARGE;
    NumForgeNumericalStatus status = NUMFORGE_NUMERICAL_OK;
    ScanWork work = {evaluate, domain, user, options, 0, NULL};
    NumForgeRootScan *output = numforge_calloc(1, sizeof(*output));
    if (output) {
        output->capacity = options->max_roots;
        output->roots = numforge_calloc(output->capacity, sizeof(*output->roots));
    }
    BigRational *current = bigrational_create(), *previous = bigrational_create(), *two_back = bigrational_create(), *step = bigrational_create(), *divisor = bigrational_create();
    BigDecimal *constant = bigdecimal_create(); Probe samples[2] = {{0}};
    samples[0].value = bigdecimal_create(); samples[1].value = bigdecimal_create();
    ScanScratch scratch = {0};
    scratch.middle.value = bigdecimal_create(); scratch.vertex.value = bigdecimal_create();
    scratch.middle.proposal_value = bigrational_create(); scratch.vertex.proposal_value = bigrational_create();
    samples[0].proposal_value = bigrational_create(); samples[1].proposal_value = bigrational_create();
    if (!output || !output->roots || !current || !previous || !two_back || !step || !divisor || !constant || !samples[0].value || !samples[1].value) status = NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    if (!scratch.middle.value || !scratch.vertex.value || !scratch.middle.proposal_value || !scratch.vertex.proposal_value || !samples[0].proposal_value || !samples[1].proposal_value) status = NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    for (size_t i = 0U; i < SC_COUNT && status == NUMFORGE_NUMERICAL_OK; ++i) { scratch.values[i] = bigrational_create(); if (!scratch.values[i]) status = NUMFORGE_NUMERICAL_OUT_OF_MEMORY; }
    int order = 0, two_sign = 0; bool two_real = false;
    if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_compare(&order, lower, upper));
    if (status == NUMFORGE_NUMERICAL_OK && order >= 0) status = NUMFORGE_NUMERICAL_INVALID_ARGUMENT;
    if (status == NUMFORGE_NUMERICAL_OK) {
        char text[3*sizeof(size_t)+1];
        (void)snprintf(text, sizeof(text), "%zu", options->segments);
        status = decimal(bigdecimal_set_string(constant, text));
    }
    if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_from_bigdecimal(divisor, constant));
    if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_sub(step, upper, lower));
    if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_div(step, step, divisor));
    if (status == NUMFORGE_NUMERICAL_OK) status = decimal(bigdecimal_set_string(constant, "2"));
    if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_from_bigdecimal(scratch.values[SC_TWO], constant));
    if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_add(scratch.values[SC_FOUR], scratch.values[SC_TWO], scratch.values[SC_TWO]));
    if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_copy(current, lower));
    work.zero = scratch.values[SC_ZERO];
    if (status == NUMFORGE_NUMERICAL_OK && options->root_options.x_tolerance) {
        status = rational(bigrational_compare(&order, options->root_options.x_tolerance, work.zero));
        if (status == NUMFORGE_NUMERICAL_OK && order <= 0) status = NUMFORGE_NUMERICAL_INVALID_ARGUMENT;
    }
    if (status == NUMFORGE_NUMERICAL_OK && options->root_options.residual_tolerance) {
        status = decimal(bigdecimal_sign(&order, options->root_options.residual_tolerance));
        if (status == NUMFORGE_NUMERICAL_OK && order <= 0) status = NUMFORGE_NUMERICAL_INVALID_ARGUMENT;
    }
    for (size_t i = 0U; i <= options->segments && status == NUMFORGE_NUMERICAL_OK && output->stop == NUMFORGE_SCAN_FINISHED; ++i) {
        if (work.evaluations >= options->max_evaluations) { output->stop = NUMFORGE_SCAN_EVALUATION_LIMIT; break; }
        status = checked(status);
        if (status == NUMFORGE_NUMERICAL_OK) status = probe(&work, current, &samples[1]);
        if (status != NUMFORGE_NUMERICAL_OK) break;
        if (samples[1].real && samples[1].exact_zero) {
            status = scan_exact(output, current, &samples[1]);
        } else if (i && samples[0].real && samples[1].real && samples[1].sign && !samples[0].exact_zero &&
            ((samples[0].sign && samples[0].sign != samples[1].sign) ||
             (i >= 2U && !samples[0].sign && two_real && two_sign && two_sign != samples[1].sign))) {
            // An approximate grid zero is not accepted by itself. Bracket it
            // using real, opposite-sign neighbours (e.g. sin(0) or ln(1)).
            status = scan_bracket(&work, output, samples[0].sign ? previous : two_back, current);
        } else if (i) status = scan_cell(&work, output, &scratch, previous, current, &samples[0], &samples[1]);
        if (status != NUMFORGE_NUMERICAL_OK || output->stop == NUMFORGE_SCAN_EVALUATION_LIMIT) break;
        if (i) { output->scanned_segments = i; if (!samples[0].real || !samples[1].real) ++output->domain_gaps; }
        two_real = samples[0].real; two_sign = samples[0].sign;
        Probe swap = samples[0]; samples[0] = samples[1]; samples[1] = swap;
        status = rational(bigrational_copy(two_back, previous));
        if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_copy(previous, current));
        if (status == NUMFORGE_NUMERICAL_OK && i < options->segments)
            status = i+1U == options->segments ? rational(bigrational_copy(current, upper)) : rational(bigrational_add(current, current, step));
    }
    status = checked(status);
    if (status == NUMFORGE_NUMERICAL_OK) { output->evaluations = work.evaluations; *result = output; output = NULL; }
    for (size_t i = 0U; i < 2U; ++i) { bigdecimal_destroy(samples[i].value); bigrational_destroy(samples[i].proposal_value); }
    for (size_t i = 0U; i < SC_COUNT; ++i) bigrational_destroy(scratch.values[i]);
    bigdecimal_destroy(scratch.middle.value); bigdecimal_destroy(scratch.vertex.value);
    bigrational_destroy(scratch.middle.proposal_value); bigrational_destroy(scratch.vertex.proposal_value);
    bigrational_destroy(current); bigrational_destroy(previous); bigrational_destroy(two_back); bigrational_destroy(step); bigrational_destroy(divisor); bigdecimal_destroy(constant);
    numforge_root_scan_destroy(output);
    return status;
}
size_t numforge_root_scan_count(const NumForgeRootScan *r) { return r ? r->count : 0; }
const NumForgeRoot *numforge_root_scan_root(const NumForgeRootScan *r, size_t index)
{ return r && index < r->count ? r->roots[index] : NULL; }
NumForgeRootScanStop numforge_root_scan_stop(const NumForgeRootScan *r)
{ return r ? r->stop : NUMFORGE_SCAN_EVALUATION_LIMIT; }
void numforge_root_scan_stats(const NumForgeRootScan *r, NumForgeRootScanStats *stats)
{
    if (!stats) return;
    memset(stats, 0, sizeof(*stats));
    if (!r) return;
    stats->evaluations = r->evaluations; stats->iterations = r->iterations;
    stats->scanned_segments = r->scanned_segments; stats->domain_gaps = r->domain_gaps;
    stats->rejected_brackets = r->rejected_brackets; stats->candidate_samples = r->candidate_samples;
    stats->candidate_roots = r->candidate_roots; stats->candidate_gaps = r->candidate_gaps;
}
