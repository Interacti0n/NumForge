#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#define NUMFORGE_DECIMAL_PROFILE_NO_REDIRECT
#include "src/runtime/instrumentation/decimal_profile.h"
#include "src/core/bigint/bigint_internal.h"
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
static DECIMAL_LOCAL double operation_totals[NUMFORGE_DECIMAL_OPERATIONS];
static DECIMAL_LOCAL size_t operation_calls[NUMFORGE_DECIMAL_OPERATIONS];

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
    memset(operation_totals, 0, sizeof(operation_totals));
    memset(operation_calls, 0, sizeof(operation_calls));
    current = NUMFORGE_DECIMAL_OTHER;
    started = clock_seconds();
    complete = started >= 0;
    active = 1;
}
void numforge_decimal_profile_stop(void) { if (active) checkpoint(); active = 0; }
int numforge_decimal_profile_complete(void) { return complete; }
double numforge_decimal_profile_seconds(NumForgeDecimalPhase phase) { return totals[phase]; }
double numforge_decimal_operation_seconds(NumForgeDecimalOperation operation) { return operation_totals[operation]; }
size_t numforge_decimal_operation_calls(NumForgeDecimalOperation operation) { return operation_calls[operation]; }
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
/* Separate probes only: these clocks never run in ordinary timing samples.
 * BigInt internals do not redirect their nested calls, so intervals are disjoint. */
static void record_operation(NumForgeDecimalOperation operation, double begin)
{
    if (active)
    {
        double end = clock_seconds();
        if (begin < 0 || end < begin) complete = 0;
        else operation_totals[operation] += end - begin;
        operation_calls[operation]++;
    }
}
#define CORE_WRAPPER(name, operation) \
    BigIntStatus numforge_decimal_##name(BigInt *out, const BigInt *a, const BigInt *b) \
    { \
        NumForgeDecimalPhase previous = numforge_decimal_enter(NUMFORGE_DECIMAL_CORE); \
        double begin = active ? clock_seconds() : 0.0; \
        BigIntStatus status = bigint_##name(out, a, b); \
        record_operation(operation, begin); \
        numforge_decimal_leave(previous); \
        return status; \
    }
CORE_WRAPPER(add, NUMFORGE_DECIMAL_ADD)
CORE_WRAPPER(sub, NUMFORGE_DECIMAL_SUB)
CORE_WRAPPER(mul, NUMFORGE_DECIMAL_MUL)
CORE_WRAPPER(div, NUMFORGE_DECIMAL_DIV)
CORE_WRAPPER(mod, NUMFORGE_DECIMAL_MOD)
CORE_WRAPPER(pow, NUMFORGE_DECIMAL_POW)
CORE_WRAPPER(gcd, NUMFORGE_DECIMAL_GCD)
BigIntStatus numforge_decimal_div_mod(BigInt *out, BigInt *rem, const BigInt *a, const BigInt *b)
{
    NumForgeDecimalPhase previous = numforge_decimal_enter(NUMFORGE_DECIMAL_CORE);
    double begin = active ? clock_seconds() : 0.0;
    BigIntStatus status = bigint_div_mod(out, rem, a, b);
    record_operation(NUMFORGE_DECIMAL_DIV_MOD, begin);
    numforge_decimal_leave(previous);
    return status;
}

BigIntStatus numforge_decimal_divmod_abs_workspace(BigInt *out, BigInt *rem,
    const BigInt *a, const BigInt *b, BigIntDivisionWorkspace *workspace)
{
    NumForgeDecimalPhase previous = numforge_decimal_enter(NUMFORGE_DECIMAL_CORE);
    double begin = active ? clock_seconds() : 0.0;
    BigIntStatus status = bigint_divmod_abs_workspace(out, rem, a, b, workspace);
    record_operation(NUMFORGE_DECIMAL_DIV_MOD, begin);
    numforge_decimal_leave(previous);
    return status;
}

BigIntStatus numforge_decimal_add_abs(BigInt *out, const BigInt *a, const BigInt *b)
{
    NumForgeDecimalPhase previous = numforge_decimal_enter(NUMFORGE_DECIMAL_CORE);
    double begin = active ? clock_seconds() : 0.0;
    BigIntStatus status = bigint_add_abs(out, a, b);
    record_operation(NUMFORGE_DECIMAL_ADD, begin);
    numforge_decimal_leave(previous);
    return status;
}

BigIntStatus numforge_decimal_subtract_abs(BigInt *out, const BigInt *a, const BigInt *b)
{
    NumForgeDecimalPhase previous = numforge_decimal_enter(NUMFORGE_DECIMAL_CORE);
    double begin = active ? clock_seconds() : 0.0;
    BigIntStatus status = bigint_subtract_abs(out, a, b);
    record_operation(NUMFORGE_DECIMAL_SUB, begin);
    numforge_decimal_leave(previous);
    return status;
}
