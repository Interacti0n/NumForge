#ifndef NUMFORGE_KARATSUBA_EXPERIMENT_H
#define NUMFORGE_KARATSUBA_EXPERIMENT_H
#include "../src/bigint/bigint_internal.h"
/* Benchmark-only, atomic aliases, one per-call bounded scratch arena. */
BigIntStatus benchmark_karatsuba(BigInt *out, const BigInt *a, const BigInt *b,
                               size_t threshold);
#ifdef NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING
bool benchmark_karatsuba_fault_checks(void);
#endif
#endif
