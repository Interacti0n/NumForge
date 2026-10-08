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
CalculatorStatus numforge_web_conversion_response(const ApplicationConversion *entry,
    bool include_snapshot, char **response)
{
    if (entry == NULL || response == NULL) return CALCULATOR_NULL_ARGUMENT;
    *response = NULL;
    char *snapshot = NULL;
    BigRational *exact = NULL;
    CalculatorStatus status = CALCULATOR_OK;
    if (include_snapshot) {
        if (entry->value.kind == CALCULATOR_VALUE_DECIMAL) {
            exact = bigrational_create();
            if (exact == NULL) return CALCULATOR_OUT_OF_MEMORY;
            status = rational_status(bigrational_from_bigdecimal(exact, entry->value.number));
        }
        if (status == CALCULATOR_OK)
            status = rational_status(bigrational_to_string(exact != NULL ? exact : entry->value.rational, &snapshot));
        bigrational_destroy(exact);
        if (status != CALCULATOR_OK) return status;
        if (strlen(snapshot) > CALCULATOR_MAX_OUTPUT_BYTES) { free(snapshot); return CALCULATOR_VALUE_TOO_LARGE; }
    }
    const NumForgeUnitInfo *to = numforge_unit_find(entry->to);
    JsonBuffer buffer = { numforge_malloc(UNIT_JSON_CAPACITY), 0, true };
    if (buffer.data == NULL) { free(snapshot); return CALCULATOR_OUT_OF_MEMORY; }
    json_append(&buffer, "{\"ok\":true,\"result\":"); json_string(&buffer, entry->display);
    json_append(&buffer, ",\"unit\":"); json_string(&buffer, entry->to);
    json_append(&buffer, ",\"symbol\":"); json_string(&buffer, to->symbol);
    json_append(&buffer, ",\"input_approximate\":%s,\"factor_approximate\":%s",
        entry->input_approximate ? "true" : "false", entry->factor_approximate ? "true" : "false");
    if (snapshot != NULL) {
        json_append(&buffer, ",\"value\":{\"kind\":\"%s\",\"text\":",
            entry->value.kind == CALCULATOR_VALUE_DECIMAL ? "decimal_approximation" : "rational");
        json_string(&buffer, snapshot);
        json_append(&buffer, ",\"unit\":"); json_string(&buffer, entry->to);
        json_append(&buffer, ",\"precision\":%lld}", (long long)entry->value.context.division_scale);
    }
    json_append(&buffer, "}");
    free(snapshot);
    if (!buffer.valid) { free(buffer.data); return CALCULATOR_VALUE_TOO_LARGE; }
    *response = buffer.data;
    return CALCULATOR_OK;
}
CalculatorStatus numforge_web_convert(const CalculatorSession *session,
    const char *input, const NumForgeConversionOptions *options,
    char **response, CalculatorError *error, const char **code)
{
    if (response != NULL) *response = NULL;
    if (options == NULL || response == NULL || code == NULL) return CALCULATOR_NULL_ARGUMENT;
    if (options->precision != options->context.division_scale) {
        *code = "invalid_options";
        calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, 0);
        return CALCULATOR_INVALID_ARGUMENT;
    }
    ApplicationConversion entry = {0};
    bool owner = numforge_budget_begin((uint64_t)options->context.time_limit_ms,
        CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    CalculatorStatus status = application_conversion_compute(session, input, options->from,
        options->to, &options->context, &entry, error, code);
    if (status == CALCULATOR_OK) status = numforge_web_conversion_response(&entry, options->snapshot, response);
    application_conversion_destroy(&entry);
    if (status != CALCULATOR_OK) calculator_error_set(error, status, error == NULL ? 0 : error->offset);
    if (status == CALCULATOR_TIME_LIMIT) *code = "time_limit";
    else if (status == CALCULATOR_VALUE_TOO_LARGE) *code = "value_too_large";
    else if (status == CALCULATOR_OUT_OF_MEMORY) *code = "out_of_memory";
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
