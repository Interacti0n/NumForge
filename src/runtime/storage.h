#ifndef NUMFORGE_RUNTIME_STORAGE_H
#define NUMFORGE_RUNTIME_STORAGE_H
#include <stddef.h>
#include <stdint.h>
/* Saturating arithmetic for private retained-storage accounting. */
static inline size_t numforge_storage_add(size_t a, size_t b)
{
    return b > SIZE_MAX - a ? SIZE_MAX : a + b;
}
#endif
