#include <numforge/numerical.h>
#include "src/runtime/numforge_alloc.h"
#include <stdlib.h>

typedef struct Interval { size_t left, depth; } Interval;

void numforge_refinement_options_init(NumForgeRefinementOptions *options)
{
    if (!options) return;
    *options = (NumForgeRefinementOptions){2048, 12, 0, 0};
}
NumForgeNumericalStatus numforge_refine_intervals(NumForgeRefinementStats *result,
    size_t initial_points, const NumForgeRefinementOptions *options,
    NumForgeRefinementProbe probe, void *user)
{
    if (!result || !probe) return NUMFORGE_NUMERICAL_NULL_ARGUMENT;
    NumForgeRefinementOptions policy;
    if (options) policy = *options;
    else numforge_refinement_options_init(&policy);
    options = &policy;
    if (initial_points < 2 || options->max_points < initial_points ||
        options->max_points > SIZE_MAX / sizeof(Interval)) return NUMFORGE_NUMERICAL_INVALID_ARGUMENT;
    if (!numforge_budget_check()) return NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    NumForgeRefinementStats stats = {initial_points, 0, 0, 0};
    size_t head = 0, pending = initial_points - 1, max_bytes = 0;
    uint64_t max_ms = 0;
    Interval *queue = NULL;
    NumForgeNumericalStatus status = NUMFORGE_NUMERICAL_OK;
    while (pending) {
        if (!numforge_budget_check()) { status = NUMFORGE_NUMERICAL_OUT_OF_MEMORY; break; }
        size_t bytes = numforge_budget_available_bytes();
        uint64_t ms = numforge_budget_available_ms();
        if (stats.points >= options->max_points) { stats.limits |= NUMFORGE_REFINEMENT_POINT_LIMIT; break; }
        if (bytes <= options->reserve_bytes || max_bytes > (bytes - options->reserve_bytes) / 2 ||
            ms <= options->reserve_ms || max_ms > (ms - options->reserve_ms) / 2) {
            stats.limits |= NUMFORGE_REFINEMENT_RESOURCE_LIMIT; break;
        }
        if (!queue) {
            if (options->max_points * sizeof(*queue) > bytes - options->reserve_bytes) {
                stats.limits |= NUMFORGE_REFINEMENT_RESOURCE_LIMIT; break;
            }
            queue = numforge_calloc(options->max_points, sizeof(*queue));
            if (!queue) { status = NUMFORGE_NUMERICAL_OUT_OF_MEMORY; break; }
            for (size_t i = 0; i < pending; ++i) queue[i] = (Interval){i, 0};
            /* Queue storage consumes the inherited budget too. Recheck admission
             * before invoking any callback, including the first one. */
            continue;
        }
        Interval current = queue[head]; head = (head + 1) % options->max_points; --pending;
        if (current.depth >= options->max_depth) {
            stats.limits |= NUMFORGE_REFINEMENT_DEPTH_LIMIT; ++stats.unresolved_intervals; continue;
        }
        NumForgeRefinementDecision decision = NUMFORGE_REFINEMENT_SPLIT;
        status = probe(&decision, current.left, current.depth, user);
        if (!numforge_budget_check()) status = NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
        if (status != NUMFORGE_NUMERICAL_OK) break;
        if (decision != NUMFORGE_REFINEMENT_SPLIT && decision != NUMFORGE_REFINEMENT_ACCEPT &&
            decision != NUMFORGE_REFINEMENT_SKIP) { status = NUMFORGE_NUMERICAL_INVALID_ARGUMENT; break; }
        ++stats.points; ++stats.probes;
        size_t after_bytes = numforge_budget_available_bytes();
        uint64_t after_ms = numforge_budget_available_ms();
        if (bytes >= after_bytes && bytes - after_bytes > max_bytes) max_bytes = bytes - after_bytes;
        if (ms >= after_ms && ms - after_ms > max_ms) max_ms = ms - after_ms;
        for (size_t i = 0; i < pending; ++i) {
            Interval *item = &queue[(head + i) % options->max_points];
            if (item->left > current.left) ++item->left;
        }
        if (decision == NUMFORGE_REFINEMENT_SPLIT) {
            /* Disjoint active edges guarantee pending <= points-1 < max_points. */
            queue[(head + pending++) % options->max_points] = (Interval){current.left, current.depth + 1};
            queue[(head + pending++) % options->max_points] = (Interval){current.left + 1, current.depth + 1};
        }
    }
    free(queue);
    if (!numforge_budget_check()) status = NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    if (status == NUMFORGE_NUMERICAL_OK) { stats.unresolved_intervals += pending; *result = stats; }
    return status;
}
