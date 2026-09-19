#ifndef NUMFORGE_BENCHMARK_CLOCK_H
#define NUMFORGE_BENCHMARK_CLOCK_H

/* Monotonic elapsed seconds; negative means the platform timer failed.
 * Implementation: benchmarks/benchmark_clock.c */
double benchmark_seconds(void);

#endif
