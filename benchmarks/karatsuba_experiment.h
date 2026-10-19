#ifndef NUMFORGE_KARATSUBA_EXPERIMENT_H
#define NUMFORGE_KARATSUBA_EXPERIMENT_H
#include "src/core/bigint/bigint_internal.h"
/* Benchmark-only threshold selection over the shared private numeric kernel. */
BigIntStatus benchmark_karatsuba(BigInt *out, const BigInt *a, const BigInt *b,
                               size_t threshold);
#ifdef NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING
bool benchmark_karatsuba_fault_checks(void);
bool benchmark_multiplication_fault_checks(void);
#endif
#endif
