#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE
#elif !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "benchmark_profile.h"
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <time.h>
#include <sys/resource.h>
#endif
#ifdef _MSC_VER
#define PROFILE_LOCAL __declspec(thread)
#else
#define PROFILE_LOCAL _Thread_local
#endif
#include <string.h>

static PROFILE_LOCAL double totals[NUMFORGE_PHASE_COUNT];
static PROFILE_LOCAL double started;
static PROFILE_LOCAL NumForgeProfilePhase current;
static PROFILE_LOCAL int active;
static PROFILE_LOCAL int complete;

static double profile_clock(void)
{
#ifdef _WIN32
    LARGE_INTEGER counter, frequency;
    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0 ||
        !QueryPerformanceCounter(&counter)) return -1.0;
    return (double)counter.QuadPart / (double)frequency.QuadPart;
#else
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return -1.0;
    return (double)now.tv_sec + (double)now.tv_nsec / 1000000000.0;
#endif
}

static void checkpoint(void)
{
    double now = profile_clock();
    if (now < 0.0 || started < 0.0 || now < started) complete = 0;
    else totals[current] += now - started;
    started = now;
}
void numforge_profile_reset(void)
{
    memset(totals, 0, sizeof(totals));
    current = NUMFORGE_PHASE_OTHER;
    started = profile_clock();
    complete = started >= 0.0;
    active = 1;
}
void numforge_profile_stop(void)
{
    if (active) checkpoint();
    active = 0;
}
int numforge_profile_complete(void) { return complete; }
NumForgeProfilePhase numforge_profile_enter(NumForgeProfilePhase phase)
{
    NumForgeProfilePhase previous = current;
    if (active)
    {
        checkpoint();
        current = phase;
    }
    return previous;
}
void numforge_profile_leave(NumForgeProfilePhase previous)
{
    if (active)
    {
        checkpoint();
        current = previous;
    }
}
double numforge_profile_seconds(NumForgeProfilePhase phase) { return totals[phase]; }

unsigned long long numforge_profile_process_peak(void)
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters;
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) return 0;
    return (unsigned long long)counters.PeakWorkingSetSize;
#else
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) != 0) return 0;
#ifdef __APPLE__
    return (unsigned long long)usage.ru_maxrss;
#else
    return (unsigned long long)usage.ru_maxrss * 1024ULL;
#endif
#endif
}
