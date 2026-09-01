#include "numforge_alloc.h"

#ifdef NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING

#include <stdlib.h>

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

static bool numforge_allocator_should_fail(void)
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

void *numforge_malloc(size_t size)
{
    return numforge_allocator_should_fail() ? NULL : malloc(size);
}

void *numforge_calloc(size_t count, size_t size)
{
    return numforge_allocator_should_fail() ? NULL : calloc(count, size);
}

void *numforge_realloc(void *memory, size_t size)
{
    return numforge_allocator_should_fail() ? NULL : realloc(memory, size);
}

void numforge_test_allocator_begin(size_t failure_index)
{
    numforge_allocator_active = true;
    numforge_allocator_failed = false;
    numforge_allocator_failure_index = failure_index;
    numforge_allocator_call_count = 0U;
}

void numforge_test_allocator_end(void)
{
    numforge_allocator_active = false;
    numforge_allocator_failed = false;
    numforge_allocator_failure_index = 0U;
    numforge_allocator_call_count = 0U;
}

size_t numforge_test_allocator_call_count(void)
{
    return numforge_allocator_call_count;
}

bool numforge_test_allocator_did_fail(void)
{
    return numforge_allocator_failed;
}

#endif
