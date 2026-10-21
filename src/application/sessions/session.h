#ifndef NUMFORGE_CALCULATOR_SESSION_H
#define NUMFORGE_CALCULATOR_SESSION_H

#include "src/engine/api/calculator_internal.h"
#include "src/application/conversions/conversion.h"
#include "src/application/retention.h"

/*
 * Private client state shared by CLI and web. Zero-initialize and destroy at
 * the end of the session. Entries own values; ans borrows the newest entry
 * or owns detached_answer after clearing history. Confirmed conversions own
 * another independent bounded FIFO. An optional retention policy reserves the
 * complete prospective heap before changing confirmed values or preview state.
 * Destruction frees values but preserves an attached policy; stores detach it
 * before reclaiming a slot. Standalone zero-initialized sessions have no attached
 * retained-memory limit; ordinary per-operation budgets still apply.
 * No persistence or thread safety. Revisions identify requests, not values.
 *
 * Implementation: src/application/sessions/session.c
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
    ApplicationRetention retention;
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
/* Owned heap only: all values, displays, detached ans and conversion history.
 * Fixed inline session storage and temporary request work are separate. */
size_t calculator_session_storage_bytes(const CalculatorSession *session);
/* Reconcile after an operation that only releases storage; cannot fail. */
void calculator_session_release_storage(CalculatorSession *session);
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
