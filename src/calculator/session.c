#include "session.h"
#include "formatter.h"

#include <numforge/runtime.h>
#include <stdlib.h>
#include <string.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Session ownership and bounded history. A confirmation prepares every owned
    object before replacing state, so allocation/formatting errors preserve ans
    and history together. Clearing the preview never changes confirmed values.
------------------------------------------------------------------------------------------------------------------------------
*/

void calculator_session_clear_preview(CalculatorSession *session)
{
    if (session != NULL)
    {
        calculator_value_destroy(&session->preview);
        session->preview_expression[0] = '\0';
    }
}

void calculator_session_destroy(CalculatorSession *session)
{
    if (session == NULL)
    {
        return;
    }

    calculator_session_clear_preview(session);
    for (size_t index = 0U; index < session->count; index++)
    {
        calculator_value_destroy(&session->history[index].value);
        free(session->history[index].display);
    }
    memset(session, 0, sizeof(*session));
}

static char *calculator_session_copy_text(const char *text)
{
    size_t length = strlen(text) + 1U;
    char *copy = numforge_malloc(length);

    if (copy != NULL)
    {
        memcpy(copy, text, length);
    }
    return copy;
}

CalculatorStatus calculator_session_compute(
    CalculatorSession *session,
    uint64_t revision,
    bool commit,
    const char *input,
    const CalculatorContext *context,
    char **result,
    CalculatorError *error,
    bool *reused
)
{
    CalculatorValue value = {0};
    CalculatorHistoryEntry *last;
    const CalculatorValue *cached;
    CalculatorStatus status = CALCULATOR_OK;
    char *display = NULL;
    bool owner;
    bool hit;
    size_t length = 0U;

    if (result != NULL)
    {
        *result = NULL;
    }
    if (reused != NULL)
    {
        *reused = false;
    }
    if (session == NULL || input == NULL || context == NULL || result == NULL)
    {
        calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0U);
        return CALCULATOR_NULL_ARGUMENT;
    }
    if (context->time_limit_ms < 0 || revision == 0U)
    {
        calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, 0U);
        return CALCULATOR_INVALID_ARGUMENT;
    }
    calculator_error_clear(error);
    while (length <= CALCULATOR_MAX_INPUT_BYTES && input[length] != '\0')
    {
        length++;
    }
    if (length > CALCULATOR_MAX_INPUT_BYTES)
    {
        calculator_error_set(error, CALCULATOR_VALUE_TOO_LARGE, length);
        return CALCULATOR_VALUE_TOO_LARGE;
    }

    owner = numforge_budget_begin((uint64_t)context->time_limit_ms,
        CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    last = session->count == 0U ? NULL : &session->history[session->count - 1U];

    /* Replay the latest successful confirmation verbatim, even after previews.
     * A reused identifier with different input/settings is always rejected. */
    if (commit && last != NULL && revision == last->revision &&
        strcmp(input, last->expression) == 0 &&
        context->division_scale == last->value.context.division_scale &&
        context->output_scale == last->value.context.output_scale &&
        context->rounding == last->value.context.rounding &&
        context->angle_unit == last->value.context.angle_unit &&
        context->significant_division == last->value.context.significant_division)
    {
        *result = calculator_session_copy_text(last->display);
        status = *result == NULL ? CALCULATOR_OUT_OF_MEMORY : CALCULATOR_OK;
        status = calculator_budget_status(status);
    }
    else if (revision <= session->revision)
    {
        status = CALCULATOR_STALE_REQUEST;
    }
    else
    {
        session->revision = revision;
        hit = strcmp(input, session->preview_expression) == 0 &&
            calculator_value_matches(&session->preview, context);
        cached = &session->preview;
        if (!hit && last != NULL && !last->value.uses_answer &&
            strcmp(input, last->expression) == 0 && calculator_value_matches(&last->value, context))
        {
            cached = &last->value;
            hit = true;
        }
        if (hit && !commit)
        {
            status = calculator_format_result(cached->number, context, result);
            status = calculator_budget_status(status);
            if (status != CALCULATOR_OK)
            {
                free(*result);
                *result = NULL;
            }
            if (reused != NULL)
            {
                *reused = status == CALCULATOR_OK;
            }
            calculator_error_set(error, status, 0U);
            if (owner)
            {
                numforge_budget_end();
            }
            return status;
        }
        if (hit)
        {
            value.number = bigdecimal_create();
            if (value.number == NULL || bigdecimal_copy(value.number, cached->number) != BIGDECIMAL_OK)
            {
                status = CALCULATOR_OUT_OF_MEMORY;
            }
            value.context = *context;
            value.independent = cached->independent;
            value.uses_answer = cached->uses_answer;
        }
        else
        {
            status = calculator_compute_value_with_answer(input, context,
                last == NULL ? NULL : last->value.number, &value, error);
        }
        if (status == CALCULATOR_OK)
        {
            status = calculator_format_result(value.number, context, result);
        }
        if (status == CALCULATOR_OK && commit)
        {
            display = calculator_session_copy_text(*result);
            if (display == NULL)
            {
                status = CALCULATOR_OUT_OF_MEMORY;
            }
        }
        status = calculator_budget_status(status);
        if (status == CALCULATOR_OK)
        {
            calculator_session_clear_preview(session);
            if (commit)
            {
                if (session->count == CALCULATOR_HISTORY_CAPACITY)
                {
                    calculator_value_destroy(&session->history[0].value);
                    free(session->history[0].display);
                    memmove(session->history, session->history + 1U,
                        (CALCULATOR_HISTORY_CAPACITY - 1U) * sizeof(*session->history));
                    session->count--;
                }
                last = &session->history[session->count++];
                last->value = value;
                last->display = display;
                display = NULL;
                last->revision = revision;
                memcpy(last->expression, input, length + 1U);
            }
            else
            {
                session->preview = value;
                memcpy(session->preview_expression, input, length + 1U);
            }
            value.number = NULL;
            if (reused != NULL)
            {
                *reused = hit;
            }
        }
    }

    calculator_value_destroy(&value);
    free(display);
    if (status != CALCULATOR_OK)
    {
        free(*result);
        *result = NULL;
    }
    calculator_error_set(error, status, error == NULL || status == CALCULATOR_OK ? 0U : error->offset);
    if (owner)
    {
        numforge_budget_end();
    }
    return status;
}
