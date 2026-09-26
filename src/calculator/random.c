#include "random.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#endif

static uint64_t calculator_random_next(uint64_t *state)
{
    uint64_t value = *state;
    value ^= value >> 12;
    value ^= value << 25;
    value ^= value >> 27;
    *state = value;
    return value * UINT64_C(2685821657736338717);
}

uint64_t calculator_random_seed(void)
{
    static uint64_t sequence = UINT64_C(0x9e3779b97f4a7c15);
    uint64_t value;
#ifdef _WIN32
    if (BCryptGenRandom(NULL, (PUCHAR)&value, (ULONG)sizeof(value),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0 && value != 0U)
    {
        return value;
    }
#else
    FILE *source = fopen("/dev/urandom", "rb");
    if (source != NULL)
    {
        bool complete = fread(&value, 1U, sizeof(value), source) == sizeof(value);
        (void)fclose(source);
        if (complete && value != 0U)
        {
            return value;
        }
    }
#endif
    /* Portable fallback only when the platform entropy source is unavailable. */
    value = (uint64_t)time(NULL) ^ ((uint64_t)clock() << 32) ^
        (uint64_t)(uintptr_t)&sequence;
    sequence += UINT64_C(0x9e3779b97f4a7c15);
    value += sequence;
    value ^= value >> 30;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27;
    value *= UINT64_C(0x94d049bb133111eb);
    value ^= value >> 31;
    return value == 0U ? UINT64_C(0x2545f4914f6cdd1d) : value;
}

void calculator_random_decimal(uint64_t *state, char text[37])
{
    text[0] = '0';
    text[1] = '.';
    for (size_t index = 0U; index < 34U; index++)
    {
        uint64_t sample;
        /* Rejection removes modulo bias from the 64-bit draw. */
        do
        {
            sample = calculator_random_next(state);
        } while (sample >= UINT64_MAX - (UINT64_MAX % UINT64_C(10)));
        text[index + 2U] = (char)('0' + sample % UINT64_C(10));
    }
    text[36] = '\0';
}
