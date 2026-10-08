#ifndef NUMFORGE_CALCULATOR_SESSION_H
#define NUMFORGE_CALCULATOR_SESSION_H

#include "calculator_internal.h"
#include "conversion.h"

/*
------------------------------------------------------------------------------------------------------------------------------
    Private client state shared by CLI and web. Zero-initialize and destroy at
    the end of the session. Entries own values; ans borrows the newest entry
    or owns detached_answer after clearing history. Confirmed conversions own
    another independent bounded FIFO; its storage is not included below.
    Each retained coefficient was allocated under the 128 KiB single-allocation
    bound. Sixteen entries (value, <=64 KiB display, <=4096-byte input) retain
    less than 4 MiB including object overhead. Preview has one separate value.
    Up to 32 variable snapshots add their separately owned typed values; the
    history estimate does not include them. The same per-allocation bound applies.
    No persistence or thread safety. Revisions identify requests, not values.

    Implementation: src/application/session.c
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
    CalculatorValue detached_answer; /* owns ans after clearing history */
    CalculatorHistoryEntry history[CALCULATOR_HISTORY_CAPACITY];
    size_t count;
    uint64_t history_sequence;
    uint64_t revision;
    CalculatorValue preview;
    char preview_expression[CALCULATOR_MAX_INPUT_BYTES + 1U];
    uint64_t random_state;
    uint64_t preview_random_start;
    uint64_t preview_random_state;
    bool random_initialized;
    CalculatorVariable variables[CALCULATOR_VARIABLE_CAPACITY];
    size_t variable_count;
    uint64_t deletion_revision;
    char deleted_variable[CALCULATOR_VARIABLE_NAME_BYTES + 1U];
    uint64_t lifecycle_revision;
    unsigned int lifecycle_action;
    bool released;
    ApplicationConversion conversions[APPLICATION_CONVERSION_CAPACITY];
    size_t conversion_count;
    uint64_t conversion_sequence;
    uint64_t conversion_revision;
    uint64_t conversion_clear_revision;
} CalculatorSession;

typedef enum CalculatorSessionAction {
    CALCULATOR_SESSION_RESET = 1,
    CALCULATOR_SESSION_CLEAR_HISTORY,
    CALCULATOR_SESSION_RELEASE
} CalculatorSessionAction;

const CalculatorValue *calculator_session_answer(const CalculatorSession *session);
/* Ordered/retryable lifecycle mutations. Clearing history preserves ans. */
CalculatorStatus calculator_session_mutate(CalculatorSession *session,
    uint64_t revision, CalculatorSessionAction action);

void calculator_session_destroy(CalculatorSession *session);
void calculator_session_clear_preview(CalculatorSession *session);
/* Idempotent deletion at one revision; preserves ans, history and RNG. */
CalculatorStatus calculator_session_delete_variable(CalculatorSession *session,
    uint64_t revision, const char *name, CalculatorError *error);
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
