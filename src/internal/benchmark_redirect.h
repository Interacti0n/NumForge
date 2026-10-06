#ifndef NUMFORGE_BENCHMARK_REDIRECT_H
#define NUMFORGE_BENCHMARK_REDIRECT_H

/* Forced include for opt-in benchmark targets only. System pointers retain
 * their standard layout and remain valid arguments to ordinary free(). */
#include <stdlib.h>
#include "numforge_alloc.h"
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
#define malloc numforge_stats_malloc
#define calloc numforge_stats_calloc
#define realloc numforge_stats_realloc
#define free numforge_stats_free
#endif
#endif
