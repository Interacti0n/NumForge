#include "../src/internal/numforge_alloc.h"
#include "../src/internal/benchmark_profile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition); return 1; } } while (0)

int main(void)
{
    char *memory;
    void *replacement;
    NumForgeProfilePhase previous;
    CHECK(numforge_alloc_stats_track(true));
    memory = malloc(16U);
    CHECK(memory != NULL);
    memset(memory, 'x', 16U);
    CHECK(numforge_alloc_stats_live() == 16U);
    CHECK(!numforge_alloc_stats_track(false));
    numforge_alloc_stats_reset();
    CHECK(numforge_alloc_stats_calls() == 0U);
    CHECK(numforge_alloc_stats_peak() == 16U);
    memory = realloc(memory, 32U);
    CHECK(memory != NULL && memory[0] == 'x');
    CHECK(numforge_alloc_stats_live() == 32U && numforge_alloc_stats_peak() == 32U);
    memory = realloc(memory, 8U);
    CHECK(memory != NULL && numforge_alloc_stats_live() == 8U);
    CHECK(numforge_budget_begin(UINT64_MAX, 0U, 0U));
    replacement = numforge_realloc(memory, 64U);
    CHECK(replacement == NULL && memory[0] == 'x');
    CHECK(numforge_alloc_stats_live() == 8U);
    numforge_budget_end();
    CHECK(numforge_calloc(SIZE_MAX, 2U) == NULL);
    free(NULL);
    free(memory);
    CHECK(numforge_alloc_stats_live() == 0U && numforge_alloc_stats_complete());
    /* Exercise a growth large enough to usually move the allocation, without
     * assuming a particular allocator's address-selection policy. */
    memory = malloc(16U);
    CHECK(memory != NULL);
    memset(memory, 'm', 16U);
    replacement = malloc(128U * 1024U);
    CHECK(replacement != NULL);
    memory = realloc(memory, 1024U * 1024U);
    CHECK(memory != NULL && memory[0] == 'm' && memory[15] == 'm');
    CHECK(numforge_alloc_stats_live() == 1152U * 1024U);
    free(memory); free(replacement);
    CHECK(numforge_alloc_stats_live() == 0U);
    memory = calloc(4U, 8U);
    CHECK(memory != NULL && memory[0] == 0);
    CHECK(numforge_alloc_stats_live() == 32U);
    free(memory);
    CHECK(numforge_alloc_stats_track(false));
    memory = malloc(4U); /* allocations before the tracking scope are excluded */
    CHECK(numforge_alloc_stats_track(true));
    free(memory);
    CHECK(numforge_alloc_stats_live() == 0U && numforge_alloc_stats_complete());
    CHECK(numforge_alloc_stats_track(false));
    numforge_profile_reset();
    previous = numforge_profile_enter(NUMFORGE_PHASE_FORMAT);
    {
        NumForgeProfilePhase nested = numforge_profile_enter(NUMFORGE_PHASE_CONVERT);
        numforge_profile_leave(nested);
    }
    numforge_profile_leave(previous);
    numforge_profile_stop();
    CHECK(numforge_profile_complete());
    CHECK(numforge_profile_seconds(NUMFORGE_PHASE_FORMAT) >= 0.0);
    CHECK(numforge_profile_seconds(NUMFORGE_PHASE_CONVERT) >= 0.0);
    puts("Allocation tracking and nested phase checks passed");
    return 0;
}
