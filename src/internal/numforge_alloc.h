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

void *numforge_malloc(size_t size);
void *numforge_calloc(size_t count, size_t size);
void *numforge_realloc(void *memory, size_t size);

typedef enum NumForgeBudgetFailure
{
    NUMFORGE_BUDGET_OK,
    NUMFORGE_BUDGET_TIME,
    NUMFORGE_BUDGET_MEMORY
} NumForgeBudgetFailure;

uint64_t numforge_monotonic_ms(void);
/* Nested pipeline stages reuse their caller's budget. Only its owner ends it.
 * Allocation volume is cumulative, not live memory: free() stays standard. */
bool numforge_budget_begin(uint64_t milliseconds, size_t allocation_bytes, size_t single_allocation);
bool numforge_budget_check(void);
NumForgeBudgetFailure numforge_budget_failure(void);
void numforge_budget_end(void);

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
