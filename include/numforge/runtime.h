#ifndef NUMFORGE_RUNTIME_H
#define NUMFORGE_RUNTIME_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

/*
------------------------------------------------------------------------------------------------------------------------------
    Optional thread-local resource budgets. Numeric APIs do not start implicit
    budgets. Begin returns true only for the owner; nested scopes reuse the
    current budget and must not end it. Zero milliseconds expires immediately;
    UINT64_MAX gives a practically unbounded duration. No scope means no limits.
    SIZE_MAX disables an allocation limit. Limits count cumulative requested
    bytes, not live memory. Cancellation is cooperative; numeric calls report
    allocation failure and budget_failure distinguishes time/memory exhaustion.
    Successful begin must be paired with end by the same thread. Queries never
    clear the failure. Objects shared between threads require external locking.

    Budget-aware allocation helpers retain malloc/calloc/realloc ownership;
    use standard free(). Failed realloc preserves the original allocation.
    No resource scope makes arithmetic constant-time or suitable for cryptography.
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


#ifdef __cplusplus
}
#endif
#endif
