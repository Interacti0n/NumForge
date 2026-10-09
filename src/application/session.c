#include "session.h"
#include "formatter.h"
#include "random.h"
#include "constants.h"
#include "functions.h"

#include <numforge/runtime.h>
#include <stdlib.h>
#include <string.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Application session ownership and bounded history. A confirmation prepares every owned
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
    calculator_value_destroy(&session->detached_answer);
    for (size_t i = 0; i < session->conversion_count; i++)
        application_conversion_destroy(&session->conversions[i]);
    for (size_t index = 0U; index < session->count; index++)
    {
        calculator_value_destroy(&session->history[index].value);
        free(session->history[index].display);
    }
    for(size_t i=0;i<session->variable_count;i++)
        calculator_value_destroy(&session->variables[i].value);
    memset(session, 0, sizeof(*session));
}

const CalculatorValue *calculator_session_answer(const CalculatorSession *session)
{
    if (session == NULL || session->released) return NULL;
    if (session->count != 0U) return &session->history[session->count - 1U].value;
    return session->detached_answer.number != NULL ? &session->detached_answer : NULL;
}

CalculatorStatus calculator_session_mutate(CalculatorSession *session,
    uint64_t revision, CalculatorSessionAction action)
{
    if (session == NULL) return CALCULATOR_NULL_ARGUMENT;
    if (revision == 0U || action < CALCULATOR_SESSION_RESET || action > CALCULATOR_SESSION_RELEASE)
        return CALCULATOR_INVALID_ARGUMENT;
    if (revision == session->revision && revision == session->lifecycle_revision &&
        (unsigned int)action == session->lifecycle_action) return CALCULATOR_OK;
    if (session->released) return CALCULATOR_SESSION_EXPIRED;
    if (revision <= session->revision) return CALCULATOR_STALE_REQUEST;
    if (action == CALCULATOR_SESSION_CLEAR_HISTORY) {
        if (session->count != 0U) {
            calculator_value_destroy(&session->detached_answer);
            session->detached_answer = session->history[session->count - 1U].value;
            memset(&session->history[session->count - 1U].value, 0, sizeof(CalculatorValue));
        }
        for (size_t i = 0; i < session->count; i++) {
            calculator_value_destroy(&session->history[i].value);
            free(session->history[i].display);
        }
        memset(session->history, 0, sizeof(session->history));
        session->count = 0U;
        calculator_session_clear_preview(session);
    } else {
        uint64_t conversion_revision = session->conversion_revision;
        calculator_session_destroy(session);
        session->conversion_revision = conversion_revision == UINT64_MAX ? UINT64_MAX : conversion_revision + 1U;
        session->conversion_clear_revision = session->conversion_revision;
        session->released = action == CALCULATOR_SESSION_RELEASE;
    }
    session->revision = revision;
    session->lifecycle_revision = revision;
    session->lifecycle_action = (unsigned int)action;
    return CALCULATOR_OK;
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

static bool session_space(char byte)
{
    return byte==' ' || byte=='\t' || byte=='\r' || byte=='\n';
}

CalculatorStatus calculator_session_delete_variable(CalculatorSession *session,
    uint64_t revision, const char *name, CalculatorError *error)
{
    CalculatorStatus status = CALCULATOR_OK;
    size_t length = name == NULL ? 0U : strlen(name);
    if (session == NULL || name == NULL) status = CALCULATOR_NULL_ARGUMENT;
    else if (session->released) status = CALCULATOR_SESSION_EXPIRED;
    else if (revision == 0U || length == 0U || length > CALCULATOR_VARIABLE_NAME_BYTES)
        status = CALCULATOR_INVALID_ARGUMENT;
    else {
        for (size_t i = 0; i < length; i++)
            if (!((name[i] >= 'a' && name[i] <= 'z') || (name[i] >= 'A' && name[i] <= 'Z')))
                status = CALCULATOR_INVALID_ARGUMENT;
    }
    if (status != CALCULATOR_OK) goto done;
    if (revision == session->revision && revision == session->deletion_revision &&
        strcmp(name, session->deleted_variable) == 0)
        goto done;
    if (revision <= session->revision) {
        status = CALCULATOR_STALE_REQUEST;
        goto done;
    }
    for (size_t i = 0; i < session->variable_count; i++) {
        if (strcmp(name, session->variables[i].name) != 0) continue;
        calculator_value_destroy(&session->variables[i].value);
        memmove(session->variables + i, session->variables + i + 1U,
            (session->variable_count - i - 1U) * sizeof(*session->variables));
        session->variable_count--;
        memset(session->variables + session->variable_count, 0, sizeof(*session->variables));
        break;
    }
    session->revision = revision;
    session->deletion_revision = revision;
    memcpy(session->deleted_variable, name, length + 1U);
    calculator_session_clear_preview(session);
done:
    calculator_error_set(error, status, 0U);
    return status;
}

/* Assignment is one top-level statement. The ordinary parser handles the RHS
 * and rejects nested/chained assignments. Names follow the existing ASCII
 * identifier grammar; constants/functions and ans remain reserved. */
static CalculatorStatus session_assignment(const char *input, char *name,
    const char **expression, CalculatorError *error)
{
    const char *equal=strchr(input,'=');
    const char *start=input, *end;
    CalculatorConstant constant;
    size_t length;
    name[0]='\0'; *expression=input;
    if(equal==NULL) return CALCULATOR_OK;
    while(session_space(*start)) start++;
    end=equal;
    while(end>start && session_space(end[-1])) end--;
    length=(size_t)(end-start);
    if(length==0U || length>CALCULATOR_VARIABLE_NAME_BYTES) goto invalid;
    for(size_t i=0;i<length;i++)
        if(!((start[i]>='a' && start[i]<='z') || (start[i]>='A' && start[i]<='Z'))) goto invalid;
    if((length==1U && start[0]=='i') || (length==3U && memcmp(start,"ans",3U)==0) ||
        calculator_function_find(start,length)!=NULL ||
        calculator_constant_from_text(start,length,&constant)) goto invalid;
    memcpy(name,start,length);name[length]='\0';
    *expression=equal+1;
    return CALCULATOR_OK;
invalid:
    calculator_error_set(error,CALCULATOR_INVALID_ARGUMENT,(size_t)(start-input));
    return CALCULATOR_INVALID_ARGUMENT;
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
    CalculatorValue assigned = {0};
    char assignment[CALCULATOR_VARIABLE_NAME_BYTES + 1U];
    const char *expression = input;
    size_t variable_slot = 0U;
    CalculatorHistoryEntry *last;
    const CalculatorValue *cached;
    CalculatorStatus status = CALCULATOR_OK;
    char *display = NULL;
    bool owner;
    bool hit;
    bool same_preview;
    uint64_t random_start;
    uint64_t random_next;
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
    if (session->released) {
        calculator_error_set(error, CALCULATOR_SESSION_EXPIRED, 0U);
        return CALCULATOR_SESSION_EXPIRED;
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
    if (commit && last != NULL && last->revision > session->deletion_revision &&
        last->revision > session->lifecycle_revision && revision == last->revision &&
        strcmp(input, last->expression) == 0 &&
        context->division_scale == last->value.context.division_scale &&
        context->output_scale == last->value.context.output_scale &&
        context->rounding == last->value.context.rounding &&
        context->notation == last->value.context.notation &&
        context->complex_form == last->value.context.complex_form &&
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
        status = session_assignment(input, assignment, &expression, error);
        if (status != CALCULATOR_OK) goto cleanup;
        if (assignment[0] != '\0')
        {
            for(variable_slot=0;variable_slot<session->variable_count;variable_slot++)
                if(strcmp(assignment,session->variables[variable_slot].name)==0) break;
            if(variable_slot==CALCULATOR_VARIABLE_CAPACITY)
            {
                status=CALCULATOR_VALUE_TOO_LARGE;
                goto cleanup;
            }
        }
        if (!session->random_initialized)
        {
            session->random_state = calculator_random_seed();
            session->random_initialized = true;
        }
        same_preview = session->preview.number != NULL &&
            strcmp(input, session->preview_expression) == 0;
        random_start = same_preview ? session->preview_random_start : session->random_state;
        random_next = random_start;
        hit = strcmp(input, session->preview_expression) == 0 &&
            calculator_value_matches(&session->preview, context);
        cached = &session->preview;
        if (!hit && last != NULL && !last->value.uses_answer && !last->value.uses_random && !last->value.uses_variables &&
            strcmp(input, last->expression) == 0 && calculator_value_matches(&last->value, context))
        {
            cached = &last->value;
            hit = true;
        }
        if (hit && !commit)
        {
            status = calculator_format_value(cached, context, result);
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
            status = calculator_value_copy(&value, cached);
            value.context = *context;
            if (cached == &session->preview)
            {
                random_next = session->preview_random_state;
            }
        }
        else
        {
            status = calculator_compute_value_with_variables(expression, context,
                calculator_session_answer(session), &random_next,
                session->variables, session->variable_count, &value, error);
            if (status != CALCULATOR_OK && error != NULL)
                error->offset += (size_t)(expression - input);
        }
        if (status == CALCULATOR_OK)
        {
            status = calculator_format_value(&value, context, result);
        }
        if (status == CALCULATOR_OK && commit)
        {
            display = calculator_session_copy_text(*result);
            if (display == NULL)
            {
                status = CALCULATOR_OUT_OF_MEMORY;
            }
        }
        if (status == CALCULATOR_OK && commit && assignment[0] != '\0')
            status = calculator_value_copy(&assigned, &value);
        status = calculator_budget_status(status);
        if (status == CALCULATOR_OK)
        {
            calculator_session_clear_preview(session);
            if (commit)
            {
                calculator_value_destroy(&session->detached_answer);
                session->random_state = random_next;
                if (assignment[0] != '\0')
                {
                    calculator_value_destroy(&session->variables[variable_slot].value);
                    session->variables[variable_slot].value=assigned;
                    memset(&assigned,0,sizeof(assigned));
                    memcpy(session->variables[variable_slot].name,assignment,strlen(assignment)+1U);
                    if(variable_slot==session->variable_count) session->variable_count++;
                }
                if (session->count == CALCULATOR_HISTORY_CAPACITY)
                {
                    calculator_value_destroy(&session->history[0].value);
                    free(session->history[0].display);
                    memmove(session->history, session->history + 1U,
                        (CALCULATOR_HISTORY_CAPACITY - 1U) * sizeof(*session->history));
                    session->count--;
                }
                last = &session->history[session->count++];
                session->history_sequence++;
                last->value = value;
                last->display = display;
                display = NULL;
                last->revision = revision;
                memcpy(last->expression, input, length + 1U);
            }
            else
            {
                session->preview = value;
                session->preview_random_start = random_start;
                session->preview_random_state = random_next;
                memcpy(session->preview_expression, input, length + 1U);
            }
            value.number = NULL;
            value.integer = NULL;
            value.rational = NULL;
            value.complex_decimal = NULL;
            value.complex_rational = NULL;
            if (reused != NULL)
            {
                *reused = hit;
            }
        }
    }

cleanup:
    calculator_value_destroy(&value);
    calculator_value_destroy(&assigned);
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
