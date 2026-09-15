#ifndef NUMFORGE_ALLOC_H
#define NUMFORGE_ALLOC_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Internal allocation boundary.

    Optional thread-local application budgets leave ordinary library callers
    unrestricted. Test-enabled builds also support allocation fault injection.
------------------------------------------------------------------------------------------------------------------------------
*/

#include <numforge/runtime.h>

#ifdef NUMFORGE_ENABLE_ALLOC_STATS
/* Benchmark-only requested allocation volume; not live memory or RSS. */
void numforge_alloc_stats_reset(void);
size_t numforge_alloc_stats_calls(void);
size_t numforge_alloc_stats_bytes(void);
#endif

#ifdef NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING
/* Begin one isolated allocation test. failure_index is one-based; zero counts
 * allocation calls without injecting a failure. The test controller is
 * intentionally process-global and must only be used by single-threaded tests. */
void numforge_test_allocator_begin(size_t failure_index);
void numforge_test_allocator_end(void);
size_t numforge_test_allocator_call_count(void);
bool numforge_test_allocator_did_fail(void);
/* Deterministic deadline injection: expire at the selected budget checkpoint. */
void numforge_test_budget_expire_after(size_t checks);

#endif

#endif
