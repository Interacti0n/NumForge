#include "conversion.h"
#include "session.h"
#include "tokenizer.h"
#include <numforge/units.h>
#include <numforge/runtime.h>
#include <stdlib.h>
#include <string.h>

static CalculatorStatus rational_status(BigRationalStatus status)
{
    switch (status)
    {
        case BIGRATIONAL_OK: return CALCULATOR_OK;
        case BIGRATIONAL_OUT_OF_MEMORY: return CALCULATOR_OUT_OF_MEMORY;
        case BIGRATIONAL_VALUE_TOO_LARGE: return CALCULATOR_VALUE_TOO_LARGE;
        case BIGRATIONAL_SCALE_OVERFLOW: return CALCULATOR_SCALE_OVERFLOW;
        default: return CALCULATOR_INVALID_ARGUMENT;
    }
}
static CalculatorStatus unit_status(NumForgeUnitStatus status)
{
    switch (status)
    {
        case NUMFORGE_UNIT_OK: return CALCULATOR_OK;
        case NUMFORGE_UNIT_OUT_OF_MEMORY: return CALCULATOR_OUT_OF_MEMORY;
        case NUMFORGE_UNIT_VALUE_TOO_LARGE: return CALCULATOR_VALUE_TOO_LARGE;
        case NUMFORGE_UNIT_SCALE_OVERFLOW: return CALCULATOR_SCALE_OVERFLOW;
        default: return CALCULATOR_INVALID_ARGUMENT;
    }
}
static CalculatorStatus read_only_input(const char *input, CalculatorError *error, const char **code)
{
    size_t length = 0;
    while (length <= CALCULATOR_MAX_INPUT_BYTES && input[length]) ++length;
    if (length > CALCULATOR_MAX_INPUT_BYTES) return CALCULATOR_VALUE_TOO_LARGE;
    const char *assignment = memchr(input, '=', length);
    if (assignment != NULL)
    {
        *code = "assignment_not_allowed";
        calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, (size_t)(assignment-input));
        return CALCULATOR_INVALID_ARGUMENT;
    }
    CalculatorTokenizer tokenizer;
    CalculatorToken token;
    CalculatorStatus status = calculator_tokenizer_init(&tokenizer, input);
    while (status == CALCULATOR_OK)
    {
        status = calculator_tokenizer_next(&tokenizer, &token, error);
        if (status != CALCULATOR_OK || token.type == CALCULATOR_TOKEN_END) break;
        if (token.type == CALCULATOR_TOKEN_IDENTIFIER && token.length == 4 && memcmp(token.text, "rand", 4) == 0)
        {
            *code = "random_not_allowed";
            calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, token.offset);
            return CALCULATOR_INVALID_ARGUMENT;
        }
    }
    return status;
}
void application_conversion_destroy(ApplicationConversion *entry)
{
    if (entry == NULL) return;
    calculator_value_destroy(&entry->value);
    free(entry->display);
    memset(entry, 0, sizeof(*entry));
}

static bool conversion_matches(const ApplicationConversion *entry, const char *input,
    const char *from, const char *to, const CalculatorContext *context)
{
    const CalculatorContext *old = &entry->value.context;
    return strcmp(entry->expression, input) == 0 && strcmp(entry->from, from) == 0 &&
        strcmp(entry->to, to) == 0 && old->division_scale == context->division_scale &&
        old->output_scale == context->output_scale && old->rounding == context->rounding &&
        old->notation == context->notation && old->angle_unit == context->angle_unit &&
        old->significant_division == context->significant_division;
}

CalculatorStatus application_conversion_confirm(CalculatorSession *session,
    uint64_t revision, const char *input, const char *from, const char *to,
    const CalculatorContext *context, const ApplicationConversion **entry,
    CalculatorError *error, const char **code)
{
    if (entry != NULL) *entry = NULL;
    if (code != NULL) *code = "expression_error";
    if (session == NULL || input == NULL || from == NULL || to == NULL || context == NULL || entry == NULL || code == NULL)
        return CALCULATOR_NULL_ARGUMENT;
    if (session->released) return CALCULATOR_SESSION_EXPIRED;
    if (revision == 0U) return CALCULATOR_INVALID_ARGUMENT;
    if (revision == session->conversion_revision && session->conversion_count != 0U) {
        const ApplicationConversion *last = &session->conversions[session->conversion_count - 1U];
        if (last->revision == revision && conversion_matches(last, input, from, to, context)) {
            *entry = last;
            return CALCULATOR_OK;
        }
    }
    if (revision <= session->conversion_revision) return CALCULATOR_STALE_REQUEST;
    bool owner = numforge_budget_begin(context->time_limit_ms < 0 ? 0U : (uint64_t)context->time_limit_ms,
        CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    ApplicationConversion prepared = {0};
    CalculatorStatus status = application_conversion_compute(session, input, from, to, context,
        &prepared, error, code);
    char *text = NULL;
    if (status == CALCULATOR_OK) status = calculator_value_snapshot_text(&prepared.value, &text);
    /* Retained records must fit a single full response, including worst-case
     * escaped input and framing. Do this before changing confirmed state. */
    if (status == CALCULATOR_OK && strlen(text) + strlen(prepared.display) +
        6U * strlen(input) + 2048U >= 128U * 1024U) status = CALCULATOR_VALUE_TOO_LARGE;
    free(text);
    status = calculator_budget_status(status);
    if (status != CALCULATOR_OK) {
        application_conversion_destroy(&prepared);
        if (owner) numforge_budget_end();
        calculator_error_set(error, status, error == NULL ? 0 : error->offset);
        if (status == CALCULATOR_VALUE_TOO_LARGE) *code = "value_too_large";
        else if (status == CALCULATOR_OUT_OF_MEMORY) *code = "out_of_memory";
        else if (status == CALCULATOR_TIME_LIMIT) *code = "time_limit";
        return status;
    }
    if (session->conversion_count == APPLICATION_CONVERSION_CAPACITY) {
        application_conversion_destroy(&session->conversions[0]);
        memmove(session->conversions, session->conversions + 1U,
            (APPLICATION_CONVERSION_CAPACITY - 1U) * sizeof(ApplicationConversion));
        session->conversion_count--;
    }
    prepared.revision = revision;
    session->conversions[session->conversion_count++] = prepared;
    session->conversion_sequence++;
    session->conversion_revision = revision;
    *entry = &session->conversions[session->conversion_count - 1U];
    if (owner) numforge_budget_end();
    return CALCULATOR_OK;
}

CalculatorStatus application_conversion_clear(CalculatorSession *session, uint64_t revision)
{
    if (session == NULL) return CALCULATOR_NULL_ARGUMENT;
    if (session->released) return CALCULATOR_SESSION_EXPIRED;
    if (revision == 0U) return CALCULATOR_INVALID_ARGUMENT;
    if (revision == session->conversion_revision && revision == session->conversion_clear_revision) return CALCULATOR_OK;
    if (revision <= session->conversion_revision) return CALCULATOR_STALE_REQUEST;
    for (size_t i = 0; i < session->conversion_count; i++) application_conversion_destroy(&session->conversions[i]);
    memset(session->conversions, 0, sizeof(session->conversions));
    session->conversion_count = 0U;
    session->conversion_sequence = 0U;
    session->conversion_revision = revision;
    session->conversion_clear_revision = revision;
    return CALCULATOR_OK;
}

CalculatorStatus application_conversion_compute(const CalculatorSession *session,
    const char *input, const char *from_id, const char *to_id, const CalculatorContext *context,
    ApplicationConversion *entry, CalculatorError *error, const char **code)
{
    CalculatorValue source = {0}, converted = {0};
    BigRational *exact = NULL;
    char *display = NULL;
    CalculatorStatus status = CALCULATOR_OK;
    bool owner = false;
    if (entry != NULL) memset(entry, 0, sizeof(*entry));
    if (code != NULL) *code = "expression_error";
    calculator_error_clear(error);
    if (input == NULL || context == NULL || entry == NULL || code == NULL || from_id == NULL || to_id == NULL)
    {
        calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0);
        return CALCULATOR_NULL_ARGUMENT;
    }
    if (session != NULL && session->released) {
        *code = "session_expired";
        calculator_error_set(error, CALCULATOR_SESSION_EXPIRED, 0);
        return CALCULATOR_SESSION_EXPIRED;
    }
    if (context->division_scale < 1 || context->division_scale > CALCULATOR_MAX_OUTPUT_SCALE ||
        context->output_scale < -1 || context->output_scale > CALCULATOR_MAX_OUTPUT_SCALE ||
        context->rounding < BIGDECIMAL_ROUND_TOWARD_ZERO || context->rounding > BIGDECIMAL_ROUND_HALF_EVEN ||
        context->notation < CALCULATOR_NOTATION_AUTO || context->notation > CALCULATOR_NOTATION_FRACTION ||
        context->angle_unit < CALCULATOR_ANGLE_RADIANS || context->angle_unit > CALCULATOR_ANGLE_DEGREES ||
        context->time_limit_ms < 0)
    { *code = "invalid_options"; status = CALCULATOR_INVALID_ARGUMENT; goto cleanup; }
    const NumForgeUnitInfo *from = numforge_unit_find(from_id), *to = numforge_unit_find(to_id);
    if (from == NULL || to == NULL)
    { *code = "unknown_unit"; status = CALCULATOR_INVALID_ARGUMENT; goto cleanup; }
    if (!numforge_units_compatible(from_id, to_id))
    { *code = "incompatible_units"; status = CALCULATOR_INVALID_ARGUMENT; goto cleanup; }
    owner = numforge_budget_begin((uint64_t)context->time_limit_ms,
        CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    status = read_only_input(input, error, code);
    if (status != CALCULATOR_OK) goto cleanup;
    const CalculatorValue *answer = calculator_session_answer(session);
    CalculatorVariable empty[1] = {0};
    status = calculator_compute_value_with_variables(input, context, answer, NULL,
        session == NULL ? empty : session->variables, session == NULL ? 0 : session->variable_count,
        &source, error);
    if (status != CALCULATOR_OK) goto cleanup;
    if (source.quantity)
    {
        *code = "quantity_not_allowed";
        status = CALCULATOR_DIMENSION_ERROR;
        goto cleanup;
    }
    exact = bigrational_create();
    converted.number = bigdecimal_create();
    if (exact == NULL || converted.number == NULL) { status = CALCULATOR_OUT_OF_MEMORY; goto cleanup; }
    if (source.kind == CALCULATOR_VALUE_INTEGER)
        status = rational_status(bigrational_from_bigint(exact, source.integer));
    else if (source.kind == CALCULATOR_VALUE_RATIONAL)
        status = rational_status(bigrational_copy(exact, source.rational));
    else status = rational_status(bigrational_from_bigdecimal(exact, source.number));
    if (status != CALCULATOR_OK) goto cleanup;
    bool factor_approximate = !numforge_unit_conversion_is_exact(from_id, to_id);
    if (!factor_approximate)
    {
        status = unit_status(numforge_unit_convert_rational(exact, exact, from_id, to_id));
        if (status != CALCULATOR_OK) goto cleanup;
        converted.rational = exact; exact = NULL;
        converted.kind = CALCULATOR_VALUE_RATIONAL;
        if (source.kind == CALCULATOR_VALUE_DECIMAL)
        {
            status = rational_status(bigrational_to_bigdecimal(converted.number, converted.rational,
                context->division_scale, context->rounding));
            if (status != CALCULATOR_OK) goto cleanup;
            converted.kind = CALCULATOR_VALUE_DECIMAL;
        }
    }
    else
    {
        /* Project at working precision only on the approximate angle branch.
         * Exact conversions never pass through source.number. */
        status = rational_status(bigrational_to_bigdecimal(converted.number, exact,
            context->division_scale + CALCULATOR_ANGLE_GUARD_DIGITS, context->rounding));
        if (status == CALCULATOR_OK) status = unit_status(numforge_unit_convert_decimal(
            converted.number, converted.number, from_id, to_id,
            context->division_scale, context->rounding));
        if (status != CALCULATOR_OK) goto cleanup;
        converted.kind = CALCULATOR_VALUE_DECIMAL;
    }
    converted.context = *context;
    status = calculator_format_value(&converted, context, &display);
    if (status != CALCULATOR_OK) goto cleanup;
    status = calculator_budget_status(status);
    if (status == CALCULATOR_OK) {
        static const int dimensions[10][6] = {
            {1,0,0,0,0,0}, {2,0,0,0,0,0}, {3,0,0,0,0,0}, {0,1,0,0,0,0},
            {0,0,1,0,0,0}, {1,0,-1,0,0,0}, {0,0,0,1,0,0}, {0,0,0,1,0,0},
            {0,0,0,0,1,0}, {0,0,0,0,0,1}};
        converted.quantity = true;
        converted.temperature_point = to->quantity == NUMFORGE_UNIT_TEMPERATURE;
        memcpy(converted.dimensions, dimensions[to->quantity], sizeof(converted.dimensions));
        memcpy(converted.unit, to->id, strlen(to->id) + 1U);
        entry->value = converted;
        memset(&converted, 0, sizeof(converted));
        entry->display = display; display = NULL;
        memcpy(entry->expression, input, strlen(input) + 1U);
        memcpy(entry->from, from->id, strlen(from->id) + 1U);
        memcpy(entry->to, to->id, strlen(to->id) + 1U);
        entry->input_approximate = source.kind == CALCULATOR_VALUE_DECIMAL;
        entry->factor_approximate = factor_approximate;
    }
cleanup:
    status = calculator_budget_status(status);
    if (status != CALCULATOR_OK) {
        application_conversion_destroy(entry);
        if (error == NULL || error->status == CALCULATOR_OK) calculator_error_set(error, status, 0);
        else if (error->status != status) calculator_error_set(error, status, error->offset);
        if (status == CALCULATOR_TIME_LIMIT) *code = "time_limit";
        else if (status == CALCULATOR_VALUE_TOO_LARGE) *code = "value_too_large";
        else if (status == CALCULATOR_OUT_OF_MEMORY) *code = "out_of_memory";
    }
    free(display);
    bigrational_destroy(exact);
    calculator_value_destroy(&source);
    calculator_value_destroy(&converted);
    if (owner) numforge_budget_end();
    return status;
}
