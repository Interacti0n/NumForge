#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "numforge_alloc.h"
#include <stdlib.h>
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
/* The forced benchmark include redirects other translation units. Ledger
 * metadata and allocator implementation must use the real C allocator. */
#undef malloc
#undef calloc
#undef realloc
#undef free
#endif
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif
#ifdef _MSC_VER
#define NUMFORGE_THREAD_LOCAL __declspec(thread)
#else
#define NUMFORGE_THREAD_LOCAL _Thread_local
#endif

/*
------------------------------------------------------------------------------------------------------------------------------
    Thread-local application resource scope. Ordinary numeric library calls
    have no active scope; all returned allocations retain the standard free()
    contract. Cancellation never terminates a thread or skips normal cleanup.
------------------------------------------------------------------------------------------------------------------------------
*/

static NUMFORGE_THREAD_LOCAL bool budget_active;
static NUMFORGE_THREAD_LOCAL uint64_t budget_started;
static NUMFORGE_THREAD_LOCAL uint64_t budget_duration;
static NUMFORGE_THREAD_LOCAL size_t budget_remaining;
static NUMFORGE_THREAD_LOCAL size_t budget_single;
static NUMFORGE_THREAD_LOCAL NumForgeBudgetFailure budget_failure;
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
static NUMFORGE_THREAD_LOCAL size_t stats_calls;
static NUMFORGE_THREAD_LOCAL size_t stats_bytes;
typedef struct AllocationRecord
{
    void *memory;
    size_t size;
    struct AllocationRecord *next;
} AllocationRecord;
static NUMFORGE_THREAD_LOCAL AllocationRecord *stats_records;
static NUMFORGE_THREAD_LOCAL bool stats_tracking;
static NUMFORGE_THREAD_LOCAL bool stats_complete = true;
static NUMFORGE_THREAD_LOCAL size_t stats_live;
static NUMFORGE_THREAD_LOCAL size_t stats_peak;

static AllocationRecord **stats_find(void *memory)
{
    AllocationRecord **entry = &stats_records;
    while (*entry != NULL && (*entry)->memory != memory)
    {
        entry = &(*entry)->next;
    }
    return entry;
}

static void stats_record(void *memory, size_t size)
{
    AllocationRecord *record;
    if (!stats_tracking || memory == NULL)
    {
        return;
    }
    record = malloc(sizeof(*record));
    if (record == NULL || size > SIZE_MAX - stats_live)
    {
        free(record);
        stats_complete = false;
        return;
    }
    record->memory = memory;
    record->size = size;
    record->next = stats_records;
    stats_records = record;
    stats_live += size;
    if (stats_live > stats_peak)
    {
        stats_peak = stats_live;
    }
}

static void stats_request(size_t size)
{
    if (stats_calls < SIZE_MAX)
    {
        stats_calls++;
    }
    stats_bytes = size > SIZE_MAX - stats_bytes ? SIZE_MAX : stats_bytes + size;
}

bool numforge_alloc_stats_track(bool enabled)
{
    if (!enabled && stats_records != NULL)
    {
        return false;
    }
    if (enabled && !stats_tracking)
    {
        stats_complete = true;
    }
    stats_tracking = enabled;
    return true;
}
bool numforge_alloc_stats_complete(void) { return stats_complete; }
size_t numforge_alloc_stats_live(void) { return stats_live; }
size_t numforge_alloc_stats_peak(void) { return stats_peak; }

void *numforge_stats_malloc(size_t size)
{
    void *memory;
    stats_request(size);
    memory = malloc(size);
    stats_record(memory, size);
    return memory;
}
void *numforge_stats_calloc(size_t count, size_t size)
{
    void *memory;
    if (size != 0U && count > SIZE_MAX / size)
    {
        return NULL;
    }
    stats_request(count * size);
    memory = calloc(count, size);
    stats_record(memory, count * size);
    return memory;
}
static void *stats_resize(void *memory, size_t size)
{
    AllocationRecord **entry = stats_find(memory);
    AllocationRecord *record = *entry;
    void *replacement;
    /* realloc(p, 0) has implementation-dependent ownership. The private
     * benchmark wrapper explicitly frees it; production realloc is unchanged. */
    if (size == 0U)
    {
        numforge_stats_free(memory);
        return NULL;
    }
    replacement = realloc(memory, size);
    if (replacement == NULL)
    {
        return NULL;
    }
    if (record != NULL)
    {
        stats_live -= record->size;
        if (size > SIZE_MAX - stats_live)
        {
            stats_complete = false;
            *entry = record->next;
            free(record);
        }
        else
        {
            record->memory = replacement;
            record->size = size;
            stats_live += size;
            if (stats_live > stats_peak) stats_peak = stats_live;
        }
    }
    else
    {
        stats_record(replacement, size);
    }
    return replacement;
}
void *numforge_stats_realloc(void *memory, size_t size)
{
    stats_request(size);
    return stats_resize(memory, size);
}
void numforge_stats_free(void *memory)
{
    AllocationRecord **entry = stats_find(memory);
    if (*entry != NULL)
    {
        AllocationRecord *record = *entry;
        stats_live -= record->size;
        *entry = record->next;
        free(record);
    }
    free(memory);
}

void numforge_alloc_stats_reset(
    void
)
{
    stats_calls = stats_bytes = 0U;
    stats_peak = stats_live;
}

size_t numforge_alloc_stats_calls(
    void
)
{
    return stats_calls;
}

size_t numforge_alloc_stats_bytes(
    void
)
{
    return stats_bytes;
}
#endif
#ifdef NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING
static NUMFORGE_THREAD_LOCAL size_t budget_checks_until_expiry;

void numforge_test_budget_expire_after(
    size_t checks
)
{
    budget_checks_until_expiry = checks;
}
#endif

/*
------------------------------------------------------------------------------------------------------------------------------
    Monotonic clock and optional thread-local resource budgets.
------------------------------------------------------------------------------------------------------------------------------
*/

uint64_t numforge_monotonic_ms(
    void
)
{
#ifdef _WIN32
    return (uint64_t)GetTickCount64();
#else
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
    {
        return UINT64_MAX;
    }

    return (uint64_t)now.tv_sec * UINT64_C(1000) + (uint64_t)now.tv_nsec / UINT64_C(1000000);
#endif
}

bool numforge_budget_begin(
    uint64_t milliseconds,
    size_t allocation_bytes,
    size_t single_allocation
)
{
    if (budget_active)
    {
        return false;
    }

    budget_active = true;
    budget_started = numforge_monotonic_ms();
    budget_duration = milliseconds;
    budget_remaining = allocation_bytes;
    budget_single = single_allocation;
    budget_failure = NUMFORGE_BUDGET_OK;

    return true;
}

bool numforge_budget_check(
    void
)
{
    uint64_t now;

    if (!budget_active)
    {
        return true;
    }

    if (budget_failure != NUMFORGE_BUDGET_OK)
    {
        return false;
    }
#ifdef NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING
    if (budget_checks_until_expiry != 0U && --budget_checks_until_expiry == 0U)
    {
        budget_failure = NUMFORGE_BUDGET_TIME;

        return false;
    }
#endif
    now = numforge_monotonic_ms();

    if (now == UINT64_MAX || budget_started == UINT64_MAX || now - budget_started >= budget_duration)
    {
        budget_failure = NUMFORGE_BUDGET_TIME;

        return false;
    }

    return true;
}

NumForgeBudgetFailure numforge_budget_failure(
    void
)
{
    return budget_failure;
}

void numforge_budget_end(
    void
)
{
    budget_active = false;
    budget_failure = NUMFORGE_BUDGET_OK;
#ifdef NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING
    budget_checks_until_expiry = 0U;
#endif
}

static bool numforge_budget_allocate(
    size_t size
)
{
    if (!numforge_budget_check())
    {
        return false;
    }

    if (!budget_active)
    {
        return true;
    }

    if (size > budget_single || size > budget_remaining)
    {
        budget_failure = NUMFORGE_BUDGET_MEMORY;

        return false;
    }

    budget_remaining -= size;

    return true;
}

#ifdef NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING

/*
------------------------------------------------------------------------------------------------------------------------------
    Deterministic allocation failure injection used only by test-enabled
    builds. Exactly one selected allocation call fails during each test run;
    all cleanup continues to use the ordinary C free() contract.
------------------------------------------------------------------------------------------------------------------------------
*/

static bool numforge_allocator_active = false;
static bool numforge_allocator_failed = false;
static size_t numforge_allocator_failure_index = 0U;
static size_t numforge_allocator_call_count = 0U;

static bool numforge_allocator_should_fail(
    void
)
{
    if (!numforge_allocator_active)
    {
        return false;
    }

    numforge_allocator_call_count++;

    if (!numforge_allocator_failed && numforge_allocator_failure_index != 0U &&
        numforge_allocator_call_count == numforge_allocator_failure_index)
    {
        numforge_allocator_failed = true;

        return true;
    }

    return false;
}

void numforge_test_allocator_begin(
    size_t failure_index
)
{
    numforge_allocator_active = true;
    numforge_allocator_failed = false;
    numforge_allocator_failure_index = failure_index;
    numforge_allocator_call_count = 0U;
}

void numforge_test_allocator_end(
    void
)
{
    numforge_allocator_active = false;
    numforge_allocator_failed = false;
    numforge_allocator_failure_index = 0U;
    numforge_allocator_call_count = 0U;
}

size_t numforge_test_allocator_call_count(
    void
)
{
    return numforge_allocator_call_count;
}

bool numforge_test_allocator_did_fail(
    void
)
{
    return numforge_allocator_failed;
}

#endif

/*
------------------------------------------------------------------------------------------------------------------------------
    Budget-aware allocation wrappers. Returned memory is released with free().
------------------------------------------------------------------------------------------------------------------------------
*/

static bool numforge_allocation_allowed(
    size_t size
)
{
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    stats_request(size);
#endif
    if (!numforge_budget_allocate(size))
    {
        return false;
    }
#ifdef NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING
    if (numforge_allocator_should_fail())
    {
        return false;
    }
#endif
    return true;
}

void *numforge_malloc(
    size_t size
)
{
    void *memory = numforge_allocation_allowed(size) ? malloc(size) : NULL;
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    stats_record(memory, size);
#endif
    return memory;
}

void *numforge_calloc(
    size_t count,
    size_t size
)
{
    if (size != 0U && count > SIZE_MAX / size)
    {
        return NULL;
    }

    void *memory = numforge_allocation_allowed(count * size) ? calloc(count, size) : NULL;
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    stats_record(memory, count * size);
#endif
    return memory;
}

void *numforge_realloc(
    void *memory,
    size_t size
)
{
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    return numforge_allocation_allowed(size) ? stats_resize(memory, size) : NULL;
#else
    return numforge_allocation_allowed(size) ? realloc(memory, size) : NULL;
#endif
}
