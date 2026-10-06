#ifndef NUMFORGE_ALLOC_H
#define NUMFORGE_ALLOC_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <numforge/runtime.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Internal allocation boundary.

    Optional thread-local application budgets leave ordinary library callers
    unrestricted. Test-enabled builds also support allocation fault injection.

    Implementation: src/internal/numforge_alloc.c
------------------------------------------------------------------------------------------------------------------------------
*/

#ifdef NUMFORGE_ENABLE_ALLOC_STATS
/* Benchmark-only counters. Live tracking is opt-in, same-thread, and covers
 * redirected allocation/free calls, not allocator overhead or process RSS.
 * Reset preserves live allocations and sets the peak to the current live size.
 * Tracking may only be disabled after all tracked allocations are released. */
void *numforge_stats_malloc(size_t size);
void *numforge_stats_calloc(size_t count, size_t size);
void *numforge_stats_realloc(void *memory, size_t size);
void numforge_stats_free(void *memory);
bool numforge_alloc_stats_track(bool enabled);
bool numforge_alloc_stats_complete(void);
size_t numforge_alloc_stats_live(void);
size_t numforge_alloc_stats_peak(void);

void numforge_alloc_stats_reset(
    void
);
size_t numforge_alloc_stats_calls(
    void
);
size_t numforge_alloc_stats_bytes(
    void
);
#endif

#ifdef NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING
/* Begin one isolated allocation test. failure_index is one-based; zero counts
 * allocation calls without injecting a failure. The test controller is
 * intentionally process-global and must only be used by single-threaded tests. */

void numforge_test_allocator_begin(
    size_t failure_index
);
void numforge_test_allocator_end(
    void
);
size_t numforge_test_allocator_call_count(
    void
);
bool numforge_test_allocator_did_fail(
    void
);
/* Deterministic deadline injection: expire at the selected budget checkpoint. */

void numforge_test_budget_expire_after(
    size_t checks
);

#endif

#endif
