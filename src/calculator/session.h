#ifndef NUMFORGE_CALCULATOR_SESSION_H
#define NUMFORGE_CALCULATOR_SESSION_H

#include "calculator_internal.h"

/*
------------------------------------------------------------------------------------------------------------------------------
    Private client state shared by CLI and web. Zero-initialize and destroy at
    the end of the session. Entries own values; ans borrows the newest entry.
    Each retained coefficient was allocated under the 128 KiB single-allocation
    bound. Sixteen entries (value, <=64 KiB display, <=4096-byte input) retain
    less than 4 MiB including object overhead. Preview has one separate value.
    No persistence or thread safety. Revisions identify requests, not values.

    Implementation: src/calculator/session.c
------------------------------------------------------------------------------------------------------------------------------
*/

#define CALCULATOR_HISTORY_CAPACITY 16U

typedef struct CalculatorHistoryEntry
{
    CalculatorValue value;
    char expression[CALCULATOR_MAX_INPUT_BYTES + 1U];
    char *display;
    uint64_t revision;
} CalculatorHistoryEntry;

typedef struct CalculatorSession
{
    CalculatorHistoryEntry history[CALCULATOR_HISTORY_CAPACITY];
    size_t count;
    uint64_t revision;
    CalculatorValue preview;
    char preview_expression[CALCULATOR_MAX_INPUT_BYTES + 1U];
} CalculatorSession;

void calculator_session_destroy(CalculatorSession *session);
void calculator_session_clear_preview(CalculatorSession *session);
CalculatorStatus calculator_session_compute(
    CalculatorSession *session,
    uint64_t revision,
    bool commit,
    const char *input,
    const CalculatorContext *context,
    char **result,
    CalculatorError *error,
    bool *reused
);

#endif
