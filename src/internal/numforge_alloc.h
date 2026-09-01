#ifndef NUMFORGE_ALLOC_H
#define NUMFORGE_ALLOC_H

#include <stddef.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Internal allocation boundary.

    Normal builds map these calls directly to the C allocator. Test-enabled
    builds route them through a deterministic fault injector so every
    allocation failure path can be exercised without changing the public API.
------------------------------------------------------------------------------------------------------------------------------
*/

#ifdef NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING

#include <stdbool.h>

void *numforge_malloc(size_t size);
void *numforge_calloc(size_t count, size_t size);
void *numforge_realloc(void *memory, size_t size);

/* Begin one isolated allocation test. failure_index is one-based; zero counts
 * allocation calls without injecting a failure. The test controller is
 * intentionally process-global and must only be used by single-threaded tests. */
void numforge_test_allocator_begin(size_t failure_index);
void numforge_test_allocator_end(void);
size_t numforge_test_allocator_call_count(void);
bool numforge_test_allocator_did_fail(void);

#else

#include <stdlib.h>

#define numforge_malloc(size) malloc(size)
#define numforge_calloc(count, size) calloc((count), (size))
#define numforge_realloc(memory, size) realloc((memory), (size))

#endif

#endif
