#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "benchmark_clock.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

/*
------------------------------------------------------------------------------------------------------------------------------
    High-resolution monotonic elapsed timer shared by diagnostic benchmarks.
    No wall-clock time, CPU-time substitution, or changes to library timing.
------------------------------------------------------------------------------------------------------------------------------
*/
double benchmark_seconds(void)
{
#ifdef _WIN32
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;

    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0 ||
        !QueryPerformanceCounter(&counter))
    {
        return -1.0;
    }

    return (double)counter.QuadPart / (double)frequency.QuadPart;
#else
    struct timespec value;

    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0)
    {
        return -1.0;
    }

    return (double)value.tv_sec + (double)value.tv_nsec / 1000000000.0;
#endif
}
