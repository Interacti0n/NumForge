#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#define NUMFORGE_DECIMAL_PROFILE_NO_REDIRECT
#include "decimal_profile.h"
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif
#ifdef _MSC_VER
#define DECIMAL_LOCAL __declspec(thread)
#else
#define DECIMAL_LOCAL _Thread_local
#endif

static DECIMAL_LOCAL double totals[NUMFORGE_DECIMAL_PHASES], started;
static DECIMAL_LOCAL NumForgeDecimalPhase current;
static DECIMAL_LOCAL int active, complete;

static double clock_seconds(void)
{
#ifdef _WIN32
    LARGE_INTEGER counter, frequency;
    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0 ||
        !QueryPerformanceCounter(&counter)) return -1.0;
    return (double)counter.QuadPart / (double)frequency.QuadPart;
#else
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return -1.0;
    return (double)now.tv_sec + (double)now.tv_nsec / 1e9;
#endif
}
static void checkpoint(void)
{
    double now = clock_seconds();
    if (now < 0 || started < 0 || now < started) complete = 0;
    else totals[current] += now - started;
    started = now;
}
void numforge_decimal_profile_reset(void)
{
    memset(totals, 0, sizeof(totals));
    current = NUMFORGE_DECIMAL_OTHER;
    started = clock_seconds();
    complete = started >= 0;
    active = 1;
}
void numforge_decimal_profile_stop(void) { if (active) checkpoint(); active = 0; }
int numforge_decimal_profile_complete(void) { return complete; }
double numforge_decimal_profile_seconds(NumForgeDecimalPhase phase) { return totals[phase]; }
NumForgeDecimalPhase numforge_decimal_enter(NumForgeDecimalPhase phase)
{
    NumForgeDecimalPhase previous = current;
    if (active) { checkpoint(); current = phase; }
    return previous;
}
void numforge_decimal_leave(NumForgeDecimalPhase previous)
{
    if (active) { checkpoint(); current = previous; }
}
#define CORE_WRAPPER(name) \
    BigIntStatus numforge_decimal_##name(BigInt *out, const BigInt *a, const BigInt *b) \
    { \
        NumForgeDecimalPhase previous = numforge_decimal_enter(NUMFORGE_DECIMAL_CORE); \
        BigIntStatus status = bigint_##name(out, a, b); \
        numforge_decimal_leave(previous); \
        return status; \
    }
CORE_WRAPPER(add)
CORE_WRAPPER(sub)
CORE_WRAPPER(mul)
CORE_WRAPPER(div)
CORE_WRAPPER(mod)
CORE_WRAPPER(pow)
CORE_WRAPPER(gcd)
BigIntStatus numforge_decimal_div_mod(BigInt *out, BigInt *rem, const BigInt *a, const BigInt *b)
{
    NumForgeDecimalPhase previous = numforge_decimal_enter(NUMFORGE_DECIMAL_CORE);
    BigIntStatus status = bigint_div_mod(out, rem, a, b);
    numforge_decimal_leave(previous);
    return status;
}
