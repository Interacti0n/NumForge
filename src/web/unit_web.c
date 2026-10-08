#include "unit_web.h"
#include "tokenizer.h"
#include "../internal/numforge_alloc.h"
#include <numforge/runtime.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UNIT_JSON_CAPACITY (128U * 1024U)

const char *numforge_web_conversion_error_message(const char *code, CalculatorStatus status)
{
    if (code != NULL)
    {
        if (strcmp(code, "unknown_unit") == 0) return "unknown unit";
        if (strcmp(code, "incompatible_units") == 0) return "incompatible units";
        if (strcmp(code, "assignment_not_allowed") == 0) return "assignments are not allowed in conversion";
        if (strcmp(code, "random_not_allowed") == 0) return "random calls are not allowed in conversion";
    }
    return calculator_status_to_string(status);
}

typedef struct JsonBuffer
{
    char *data;
    size_t used;
    bool valid;
} JsonBuffer;

static void json_append(JsonBuffer *buffer, const char *format, ...)
{
    if (!buffer->valid) return;
    va_list arguments;
    va_start(arguments, format);
    int written = vsnprintf(buffer->data + buffer->used,
        UNIT_JSON_CAPACITY - buffer->used, format, arguments);
    va_end(arguments);
    if (written < 0 || (size_t)written >= UNIT_JSON_CAPACITY - buffer->used)
        buffer->valid = false;
    else buffer->used += (size_t)written;
}
static void json_string(JsonBuffer *buffer, const char *text)
{
    json_append(buffer, "\"");
    for (const unsigned char *p = (const unsigned char *)text; *p && buffer->valid; ++p)
    {
        if (*p == '"' || *p == '\\') json_append(buffer, "\\%c", *p);
        else if (*p < 32U) json_append(buffer, "\\u%04x", (unsigned int)*p);
        else json_append(buffer, "%c", *p);
    }
    json_append(buffer, "\"");
}
static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static bool decode(const char *start, size_t length, char *out, size_t capacity)
{
    size_t used = 0;
    for (size_t i = 0; i < length; ++i)
    {
        unsigned char c = (unsigned char)start[i];
        if (c == '%')
        {
            if (i + 2 >= length) return false;
            int high = hex_digit(start[i+1]), low = hex_digit(start[i+2]);
            if (high < 0 || low < 0) return false;
            c = (unsigned char)(high * 16 + low);
            i += 2;
        }
        else if (c == '+') c = ' ';
        if (c < 32U || c >= 127U || used + 1 >= capacity) return false;
        out[used++] = (char)c;
    }
    out[used] = '\0';
    return used != 0;
}
static bool number(const char *text, int64_t minimum, int64_t *result)
{
    int64_t value = 0;
    if (*text == '\0') return false;
    for (; *text; ++text)
    {
        if (*text < '0' || *text > '9' || value > (CALCULATOR_MAX_OUTPUT_SCALE - (*text-'0'))/10)
            return false;
        value = value * 10 + (*text-'0');
    }
    if (value < minimum) return false;
    *result = value;
    return true;
}
bool numforge_web_parse_conversion_options(const char *target, NumForgeConversionOptions *options)
{
    static const char prefix[] = "/api/convert?";
    static const char *keys[] = { "from", "to", "precision", "places", "rounding", "angle", "notation", "client", "snapshot" };
    static const char *roundings[] = { "toward_zero", "away_from_zero", "floor", "ceiling", "half_up", "half_even" };
    static const char *notations[] = { "auto", "plain", "scientific", "math", "fraction" };
    unsigned int seen = 0;
    if (target == NULL || options == NULL || strncmp(target, prefix, sizeof(prefix)-1) != 0) return false;
    memset(options, 0, sizeof(*options));
    calculator_context_init(&options->context);
    options->precision = CALCULATOR_DEFAULT_DIVISION_SCALE;
    const char *part = target + sizeof(prefix)-1;
    while (*part)
    {
        const char *end = strchr(part, '&');
        if (end == NULL) end = part + strlen(part);
        const char *equals = memchr(part, '=', (size_t)(end-part));
        char key[32], value[48];
        if (equals == NULL || !decode(part, (size_t)(equals-part), key, sizeof(key)) ||
            !decode(equals+1, (size_t)(end-equals-1), value, sizeof(value))) return false;
        size_t index;
        for (index = 0; index < sizeof(keys)/sizeof(keys[0]); ++index)
            if (strcmp(key, keys[index]) == 0) break;
        if (index == sizeof(keys)/sizeof(keys[0]) || (seen & (1U << index))) return false;
        seen |= 1U << index;
        switch (index)
        {
            case 0: case 1:
                if (strlen(value) >= sizeof(options->from)) return false;
                memcpy(index == 0 ? options->from : options->to, value, strlen(value) + 1);
                break;
            case 2:
                if (!number(value, 1, &options->precision)) return false;
                break;
            case 3:
                if (strcmp(value, "full") == 0) options->context.output_scale = -1;
                else if (!number(value, 0, &options->context.output_scale)) return false;
                break;
            case 4:
            {
                size_t i; for (i=0; i<6; ++i) if (strcmp(value, roundings[i]) == 0) break;
                if (i == 6) return false;
                options->context.rounding = (BigDecimalRoundingMode)i;
                break;
            }
            case 5:
                if (strcmp(value, "rad") == 0) options->context.angle_unit = CALCULATOR_ANGLE_RADIANS;
                else if (strcmp(value, "deg") == 0) options->context.angle_unit = CALCULATOR_ANGLE_DEGREES;
                else return false;
                break;
            case 6:
            {
                size_t i; for (i=0; i<5; ++i) if (strcmp(value, notations[i]) == 0) break;
                if (i == 5) return false;
                options->context.notation = (CalculatorNotation)i;
                break;
            }
            case 7:
                if (strlen(value) != 32) return false;
                for (size_t i=0; i<32; ++i)
                    if (!((value[i]>='0' && value[i]<='9') || (value[i]>='a' && value[i]<='f'))) return false;
                memcpy(options->client, value, sizeof(options->client));
                break;
            case 8:
                if (strcmp(value, "1") != 0) return false;
                options->snapshot = true;
                break;
        }
        if (*end == '\0') break;
        part = end + 1;
        if (*part == '\0') return false;
    }
    options->context.division_scale = options->precision;
    return (seen & 3U) == 3U;
}
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
CalculatorStatus numforge_web_convert(const CalculatorSession *session,
    const char *input, const NumForgeConversionOptions *options,
    char **response, CalculatorError *error, const char **code)
{
    CalculatorValue source = {0}, converted = {0};
    BigRational *exact = NULL;
    char *display = NULL, *snapshot = NULL;
    CalculatorStatus status = CALCULATOR_OK;
    bool owner = false;
    if (response != NULL) *response = NULL;
    if (code != NULL) *code = "expression_error";
    calculator_error_clear(error);
    if (input == NULL || options == NULL || response == NULL || code == NULL)
    {
        calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0);
        return CALCULATOR_NULL_ARGUMENT;
    }
    const CalculatorContext *context = &options->context;
    if (options->precision < 1 || options->precision > CALCULATOR_MAX_OUTPUT_SCALE ||
        context->output_scale < -1 || context->output_scale > CALCULATOR_MAX_OUTPUT_SCALE ||
        context->rounding < BIGDECIMAL_ROUND_TOWARD_ZERO || context->rounding > BIGDECIMAL_ROUND_HALF_EVEN ||
        context->notation < CALCULATOR_NOTATION_AUTO || context->notation > CALCULATOR_NOTATION_FRACTION ||
        context->angle_unit < CALCULATOR_ANGLE_RADIANS || context->angle_unit > CALCULATOR_ANGLE_DEGREES ||
        context->time_limit_ms < 0 || context->division_scale != options->precision)
    { *code = "invalid_options"; status = CALCULATOR_INVALID_ARGUMENT; goto cleanup; }
    const NumForgeUnitInfo *from = numforge_unit_find(options->from), *to = numforge_unit_find(options->to);
    if (from == NULL || to == NULL)
    { *code = "unknown_unit"; status = CALCULATOR_INVALID_ARGUMENT; goto cleanup; }
    if (!numforge_units_compatible(options->from, options->to))
    { *code = "incompatible_units"; status = CALCULATOR_INVALID_ARGUMENT; goto cleanup; }
    owner = numforge_budget_begin((uint64_t)context->time_limit_ms,
        CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    status = read_only_input(input, error, code);
    if (status != CALCULATOR_OK) goto cleanup;
    const CalculatorValue *answer = session != NULL && session->count != 0 ?
        &session->history[session->count-1].value : NULL;
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
    bool factor_approximate = !numforge_unit_conversion_is_exact(options->from, options->to);
    if (!factor_approximate)
    {
        status = unit_status(numforge_unit_convert_rational(exact, exact, options->from, options->to));
        if (status != CALCULATOR_OK) goto cleanup;
        converted.rational = exact; exact = NULL;
        converted.kind = CALCULATOR_VALUE_RATIONAL;
        if (source.kind == CALCULATOR_VALUE_DECIMAL)
        {
            status = rational_status(bigrational_to_bigdecimal(converted.number, converted.rational,
                options->precision, context->rounding));
            if (status != CALCULATOR_OK) goto cleanup;
            converted.kind = CALCULATOR_VALUE_DECIMAL;
        }
    }
    else
    {
        /* Project at working precision only on the approximate angle branch.
         * Exact conversions never pass through source.number. */
        status = rational_status(bigrational_to_bigdecimal(converted.number, exact,
            options->precision + CALCULATOR_ANGLE_GUARD_DIGITS, context->rounding));
        if (status == CALCULATOR_OK) status = unit_status(numforge_unit_convert_decimal(
            converted.number, converted.number, options->from, options->to,
            options->precision, context->rounding));
        if (status != CALCULATOR_OK) goto cleanup;
        converted.kind = CALCULATOR_VALUE_DECIMAL;
    }
    converted.context = *context;
    status = calculator_format_value(&converted, context, &display);
    if (status != CALCULATOR_OK) goto cleanup;
    if (options->snapshot)
    {
        /* Serialize the authoritative value, never its rounded display. A
         * decimal approximation is a finite rational snapshot with provenance. */
        if (converted.kind == CALCULATOR_VALUE_DECIMAL)
        {
            if (exact == NULL) exact = bigrational_create();
            if (exact == NULL) { status = CALCULATOR_OUT_OF_MEMORY; goto cleanup; }
            status = rational_status(bigrational_from_bigdecimal(exact, converted.number));
        }
        if (status == CALCULATOR_OK) status = rational_status(bigrational_to_string(
            converted.kind == CALCULATOR_VALUE_DECIMAL ? exact : converted.rational, &snapshot));
        if (status != CALCULATOR_OK) goto cleanup;
        if (strlen(snapshot) > CALCULATOR_MAX_OUTPUT_BYTES)
        { status = CALCULATOR_VALUE_TOO_LARGE; goto cleanup; }
    }
    JsonBuffer buffer = { numforge_malloc(UNIT_JSON_CAPACITY), 0, true };
    if (buffer.data == NULL) { status = CALCULATOR_OUT_OF_MEMORY; goto cleanup; }
    json_append(&buffer, "{\"ok\":true,\"result\":"); json_string(&buffer, display);
    json_append(&buffer, ",\"unit\":"); json_string(&buffer, to->id);
    json_append(&buffer, ",\"symbol\":"); json_string(&buffer, to->symbol);
    json_append(&buffer, ",\"input_approximate\":%s,\"factor_approximate\":%s",
        source.kind == CALCULATOR_VALUE_DECIMAL ? "true" : "false", factor_approximate ? "true" : "false");
    if (snapshot != NULL)
    {
        json_append(&buffer, ",\"value\":{\"kind\":\"%s\",\"text\":",
            converted.kind == CALCULATOR_VALUE_DECIMAL ? "decimal_approximation" : "rational");
        json_string(&buffer, snapshot);
        json_append(&buffer, ",\"unit\":"); json_string(&buffer, to->id);
        json_append(&buffer, ",\"precision\":%lld}", (long long)options->precision);
    }
    json_append(&buffer, "}");
    if (buffer.valid) *response = buffer.data;
    else { free(buffer.data); status = CALCULATOR_VALUE_TOO_LARGE; }
cleanup:
    status = calculator_budget_status(status);
    if (status != CALCULATOR_OK)
    {
        free(*response); *response = NULL;
        if (error == NULL || error->status == CALCULATOR_OK) calculator_error_set(error, status, 0);
        else if (error->status != status) calculator_error_set(error, status, error->offset);
        if (status == CALCULATOR_TIME_LIMIT) *code = "time_limit";
        else if (status == CALCULATOR_VALUE_TOO_LARGE) *code = "value_too_large";
        else if (status == CALCULATOR_OUT_OF_MEMORY) *code = "out_of_memory";
    }
    free(display);
    free(snapshot);
    bigrational_destroy(exact);
    calculator_value_destroy(&source);
    calculator_value_destroy(&converted);
    if (owner) numforge_budget_end();
    return status;
}
CalculatorStatus numforge_web_unit_catalog(char **response)
{
    static const char *quantities[] = { "length", "area", "volume", "mass", "time", "speed", "temperature", "temperature_interval", "information", "angle" };
    if (response == NULL) return CALCULATOR_NULL_ARGUMENT;
    *response = NULL;
    JsonBuffer buffer = { numforge_malloc(UNIT_JSON_CAPACITY), 0, true };
    if (buffer.data == NULL) return CALCULATOR_OUT_OF_MEMORY;
    json_append(&buffer, "{\"ok\":true,\"units\":[");
    for (size_t i=0; i<numforge_unit_count(); ++i)
    {
        const NumForgeUnitInfo *unit = numforge_unit_at(i);
        json_append(&buffer, "%s{\"id\":", i == 0 ? "" : ","); json_string(&buffer, unit->id);
        json_append(&buffer, ",\"symbol\":"); json_string(&buffer, unit->symbol);
        json_append(&buffer, ",\"quantity\":"); json_string(&buffer, quantities[unit->quantity]);
        json_append(&buffer, ",\"name_en\":"); json_string(&buffer, unit->name_en);
        json_append(&buffer, ",\"name_sk\":"); json_string(&buffer, unit->name_sk);
        json_append(&buffer, ",\"source_url\":"); json_string(&buffer, unit->source_url);
        json_append(&buffer, ",\"pi_power\":%d}", unit->pi_power);
    }
    json_append(&buffer, "]}");
    if (!buffer.valid) { free(buffer.data); return CALCULATOR_VALUE_TOO_LARGE; }
    *response = buffer.data;
    return CALCULATOR_OK;
}
