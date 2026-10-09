#include "session_api.h"
#include "unit_web.h"
#include "web_api.h"
#include "functions.h"
#include <numforge/runtime.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RESPONSE_CAPACITY (128U * 1024U)
typedef struct Json { char *data; size_t used; CalculatorStatus status; } Json;

static void append(Json *json, const char *format, ...)
{
    if (json->status != CALCULATOR_OK) return;
    va_list args; va_start(args, format);
    int count = vsnprintf(json->data + json->used, RESPONSE_CAPACITY - json->used, format, args);
    va_end(args);
    if (count < 0 || (size_t)count >= RESPONSE_CAPACITY - json->used) json->status = CALCULATOR_VALUE_TOO_LARGE;
    else json->used += (size_t)count;
}

static void string(Json *json, const char *text)
{
    append(json, "\"");
    for (const unsigned char *p = (const unsigned char *)text; *p && json->status == CALCULATOR_OK; p++) {
        if (*p == '"' || *p == '\\') append(json, "\\%c", *p);
        else if (*p < 32U) append(json, "\\u%04x", (unsigned int)*p);
        else append(json, "%c", *p);
    }
    append(json, "\"");
}

static void context(Json *json, const CalculatorContext *value)
{
    static const char *roundings[] = {"toward_zero", "away_from_zero", "floor", "ceiling", "half_up", "half_even"};
    static const char *notations[] = {"auto", "plain", "scientific", "math", "fraction"};
    append(json, "{\"precision\":%lld,\"places\":%lld,\"rounding\":\"%s\",\"notation\":\"%s\",\"angle\":\"%s\",\"significant_division\":%s}",
        (long long)value->division_scale, (long long)value->output_scale, roundings[value->rounding],
        notations[value->notation], value->angle_unit == CALCULATOR_ANGLE_DEGREES ? "deg" : "rad",
        value->significant_division ? "true" : "false");
}

static void snapshot(Json *json, const CalculatorValue *value, bool full)
{
    if (json->status != CALCULATOR_OK) return;
    if (value == NULL) { append(json, "null"); return; }
    const char *kind = value->kind == CALCULATOR_VALUE_INTEGER ? "integer" :
        value->kind == CALCULATOR_VALUE_RATIONAL ? "rational" :
        value->kind == CALCULATOR_VALUE_COMPLEX_RATIONAL ? "complex_rational" :
        value->kind == CALCULATOR_VALUE_COMPLEX_DECIMAL ? "complex_decimal_approximation" : "decimal_approximation";
    append(json, "{\"schema_version\":%u,\"kind\":\"%s\",\"full\":%s,\"text\":", calculator_value_is_complex(value) ? 2U : 1U, kind, full ? "true" : "false");
    char *text = NULL;
    if (full) {
        json->status = calculator_value_snapshot_text(value, &text);
        if (json->status == CALCULATOR_OK) string(json, text);
        free(text);
    } else append(json, "null");
    if (calculator_value_is_complex(value)) {
        append(json, ",\"components\":");
        if (full) {
            char *re=NULL,*im=NULL;
            json->status=calculator_value_complex_parts_text(value,&re,&im);
            if (json->status == CALCULATOR_OK) {
                append(json,"{\"real\":");string(json,re);append(json,",\"imaginary\":");string(json,im);append(json,"}");
            }
            free(re);free(im);
        } else append(json,"null");
    }
    append(json, ",\"approximate\":%s,\"quantity\":%s,\"temperature_point\":%s,\"dimensions\":[%d,%d,%d,%d,%d,%d],\"unit\":",
        value->kind == CALCULATOR_VALUE_DECIMAL || value->kind == CALCULATOR_VALUE_COMPLEX_DECIMAL ? "true" : "false", value->quantity ? "true" : "false",
        value->temperature_point ? "true" : "false", value->dimensions[0], value->dimensions[1], value->dimensions[2],
        value->dimensions[3], value->dimensions[4], value->dimensions[5]);
    string(json, value->unit);
    static const char *forms[] = {"cartesian","trig","exp"};
    append(json, ",\"complex_form\":\"%s\"", forms[value->context.complex_form]);
    append(json, ",\"context\":"); context(json, &value->context);
    append(json, "}");
}

enum { CLIENT, ACTION, REVISION, OFFSET, LIMIT, AT, ID, NAME, FULL,
       FROM, TO, PRECISION, PLACES, ROUNDING, ANGLE, NOTATION, QUERY_COUNT };
static const char *keys[] = {"client", "action", "revision", "offset", "limit", "at", "id", "name", "full",
    "from", "to", "precision", "places", "rounding", "angle", "notation"};
typedef struct Query { char values[QUERY_COUNT][48]; unsigned int seen; } Query;
#define BIT(field) (1U << (field))

static int hex(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static bool decode(const char *begin, const char *end, char *out)
{
    size_t count = 0;
    while (begin < end) {
        unsigned char c = (unsigned char)*begin++;
        if (c == '%') {
            if (end - begin < 2 || hex(begin[0]) < 0 || hex(begin[1]) < 0) return false;
            c = (unsigned char)(hex(begin[0]) * 16 + hex(begin[1])); begin += 2;
        } else if (c == '+') c = ' ';
        if (c < 32U || c >= 127U || count + 1U >= 48U) return false;
        out[count++] = (char)c;
    }
    out[count] = '\0'; return count != 0U;
}
static bool parse(const char *target, char path[64], Query *query)
{
    memset(query, 0, sizeof(*query));
    const char *part = strchr(target, '?');
    size_t length = part == NULL ? strlen(target) : (size_t)(part - target);
    if (length >= 64U) return false;
    memcpy(path, target, length); path[length] = '\0';
    if (part == NULL) return true;
    part++;
    if (*part == '\0') return false;
    while (*part) {
        const char *end = strchr(part, '&'); if (end == NULL) end = part + strlen(part);
        const char *equal = memchr(part, '=', (size_t)(end - part));
        char key[48], value[48];
        if (equal == NULL || !decode(part, equal, key) || !decode(equal + 1, end, value)) return false;
        size_t i; for (i = 0; i < QUERY_COUNT; i++) if (strcmp(key, keys[i]) == 0) break;
        if (i == QUERY_COUNT || (query->seen & BIT(i))) return false;
        query->seen |= BIT(i); memcpy(query->values[i], value, strlen(value) + 1U);
        if (*end == '\0') break;
        part = end + 1; if (*part == '\0') return false;
    }
    return true;
}
static bool number(const Query *query, size_t field, uint64_t fallback, uint64_t *result)
{
    if (!(query->seen & BIT(field))) { *result = fallback; return true; }
    uint64_t value = 0;
    for (const char *p = query->values[field]; *p; p++) {
        if (*p < '0' || *p > '9' || value > (UINT64_MAX - (unsigned int)(*p - '0')) / 10U) return false;
        value = value * 10U + (unsigned int)(*p - '0');
    }
    *result = value; return true;
}
static void metadata(Json *json, const CalculatorSession *session)
{
    append(json, "\"history_sequence\":\"%llu\",\"conversion_sequence\":\"%llu\",", (unsigned long long)session->history_sequence,
        (unsigned long long)session->conversion_sequence);
    append(json, "\"schema_version\":1,\"revision\":\"%llu\",\"conversion_revision\":\"%llu\",\"variable_count\":%zu,\"variable_capacity\":%u,\"history_count\":%zu,\"history_capacity\":%u,\"conversion_count\":%zu,\"conversion_capacity\":%u,\"ans_available\":%s",
        (unsigned long long)session->revision, (unsigned long long)session->conversion_revision,
        session->variable_count, CALCULATOR_VARIABLE_CAPACITY, session->count, CALCULATOR_HISTORY_CAPACITY,
        session->conversion_count, APPLICATION_CONVERSION_CAPACITY, calculator_session_answer(session) == NULL ? "false" : "true");
}

static void functions(Json *json)
{
    append(json, "{\"ok\":true,\"schema_version\":1,\"functions\":[");
    for (size_t i = 0; i < calculator_function_count(); i++) {
        const CalculatorFunction *function = calculator_function_at(i);
        const char *canonical = function->name;
        for (size_t j = 0; j < i; j++) {
            const CalculatorFunction *first = calculator_function_at(j);
            if (function->implementation != CALCULATOR_FUNCTION_PENDING && first->implementation == function->implementation) { canonical = first->name; break; }
        }
        append(json, "%s{\"name\":", i == 0 ? "" : ","); string(json, function->name);
        append(json, ",\"canonical\":"); string(json, canonical);
        append(json, ",\"minimum_arguments\":%zu,\"maximum_arguments\":%zu,\"variadic\":%s,\"implemented\":%s,\"random\":%s,\"unit_arguments\":%s}",
            function->minimum_arguments, function->maximum_arguments == 0U ? CALCULATOR_MAX_CALL_ARGUMENTS : function->maximum_arguments,
            function->maximum_arguments == 0U ? "true" : "false", function->implementation == CALCULATOR_FUNCTION_PENDING ? "false" : "true",
            function->implementation == CALCULATOR_FUNCTION_RANDOM ? "true" : "false",
            function->implementation == CALCULATOR_FUNCTION_QUANTITY ? "[1]" : function->implementation == CALCULATOR_FUNCTION_CONVERT ? "[1,2]" : "[]");
    }
    append(json, "]}");
}

static void conversion(Json *json, const ApplicationConversion *entry, bool full)
{
    const NumForgeUnitInfo *unit = numforge_unit_find(entry->to);
    append(json, "{\"id\":\"%llu\",\"expression\":", (unsigned long long)entry->revision); string(json, entry->expression);
    append(json, ",\"from\":"); string(json, entry->from);
    append(json, ",\"to\":"); string(json, entry->to);
    append(json, ",\"result\":"); string(json, entry->display);
    char *approximation = numforge_web_fraction_approximation(entry->display);
    if (approximation != NULL) {
        append(json, ",\"approx\":"); string(json, approximation);
        free(approximation);
    }
    append(json, ",\"symbol\":"); string(json, unit->symbol);
    append(json, ",\"input_approximate\":%s,\"factor_approximate\":%s,\"value\":",
        entry->input_approximate ? "true" : "false", entry->factor_approximate ? "true" : "false");
    snapshot(json, &entry->value, full); append(json, "}");
}

bool numforge_web_is_session_target(const char *target)
{
    return strncmp(target, "/api/session", 12U) == 0 || strncmp(target, "/api/conversions", 16U) == 0 ||
        strncmp(target, "/api/functions", 14U) == 0;
}

static CalculatorStatus session_request(ApplicationClientStore *store,
    const char *method, const char *target, const char *body, char **response,
    CalculatorError *error, const char **code)
{
    if (store == NULL || method == NULL || target == NULL || body == NULL || response == NULL) return CALCULATOR_NULL_ARGUMENT;
    *response = NULL;
    Query query; char path[64];
    if (!parse(target, path, &query)) return CALCULATOR_INVALID_ARGUMENT;
    bool get = strcmp(method, "GET") == 0, post = strcmp(method, "POST") == 0;
    if (!get && !post) return CALCULATOR_INVALID_ARGUMENT;
    bool registry = strcmp(path, "/api/functions") == 0;
    bool conversions = strcmp(path, "/api/conversions") == 0;
    bool summary = strcmp(path, "/api/session") == 0;
    bool variables = strcmp(path, "/api/session/variables") == 0;
    bool history = strcmp(path, "/api/session/history") == 0;
    bool value = strcmp(path, "/api/session/value") == 0;
    unsigned int allowed = BIT(CLIENT);
    if (registry) allowed = 0U;
    else if (get && (variables || history || conversions)) allowed |= BIT(OFFSET) | BIT(LIMIT) | BIT(AT) | BIT(FULL) | ((history || conversions) ? BIT(ID) : 0U);
    else if (get && value) allowed |= BIT(NAME) | BIT(AT) | BIT(FULL);
    else if (post && summary) allowed |= BIT(ACTION) | BIT(REVISION);
    else if (post && conversions) allowed |= BIT(ACTION) | BIT(REVISION) |
        BIT(FROM) | BIT(TO) | BIT(PRECISION) | BIT(PLACES) | BIT(ROUNDING) | BIT(ANGLE) | BIT(NOTATION);
    if ((!registry && !conversions && !summary && !variables && !history && !value) ||
        (query.seen & ~allowed) || (registry && !get) || (get && *body != '\0') ||
        (post && !summary && !conversions)) return CALCULATOR_INVALID_ARGUMENT;
    if (value && (!(query.seen & BIT(NAME)) || strlen(query.values[NAME]) > CALCULATOR_VARIABLE_NAME_BYTES ||
        strspn(query.values[NAME], "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ") != strlen(query.values[NAME])))
        return CALCULATOR_INVALID_ARGUMENT;
    if ((query.seen & BIT(ID)) && (query.seen & (BIT(OFFSET) | BIT(LIMIT)))) return CALCULATOR_INVALID_ARGUMENT;
    uint64_t offset, limit, at, id, revision, full;
    if (!number(&query, OFFSET, 0, &offset) || !number(&query, LIMIT, 8, &limit) ||
        !number(&query, AT, 0, &at) || !number(&query, ID, 0, &id) ||
        !number(&query, REVISION, 0, &revision) || !number(&query, FULL, value ? 1U : 0U, &full) ||
        offset > 32U || limit == 0U || limit > 32U || full > 1U) return CALCULATOR_INVALID_ARGUMENT;
    CalculatorSession *session = registry ? NULL : application_client_retained_session(store, query.values[CLIENT]);
    if (!registry && !(query.seen & BIT(CLIENT))) return CALCULATOR_INVALID_ARGUMENT;
    if (!registry && (strlen(query.values[CLIENT]) != 32U || strspn(query.values[CLIENT], "0123456789abcdef") != 32U)) return CALCULATOR_INVALID_ARGUMENT;
    if (!registry && session == NULL) return CALCULATOR_SESSION_EXPIRED;
    if (session != NULL && session->released && !(post && summary && strcmp(query.values[ACTION], "release") == 0)) return CALCULATOR_SESSION_EXPIRED;
    if (get && session != NULL && (query.seen & BIT(AT)) && at != (conversions ? session->conversion_revision : session->revision)) return CALCULATOR_STALE_REQUEST;
    bool owner = numforge_budget_begin(CALCULATOR_DEFAULT_TIME_LIMIT_MS, CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    Json json = {numforge_malloc(RESPONSE_CAPACITY), 0, CALCULATOR_OK};
    CalculatorStatus status = json.data == NULL ? CALCULATOR_OUT_OF_MEMORY : CALCULATOR_OK;
    if (status != CALCULATOR_OK) goto done;
    if (registry) { functions(&json); goto done; }
    const ApplicationConversion *confirmed = NULL;
    if (post) {
        if (revision == 0U || !(query.seen & BIT(ACTION))) { status = CALCULATOR_INVALID_ARGUMENT; goto done; }
        if (summary) {
            if (*body != '\0') { status = CALCULATOR_INVALID_ARGUMENT; goto done; }
            CalculatorSessionAction action;
            if (strcmp(query.values[ACTION], "reset") == 0) action = CALCULATOR_SESSION_RESET;
            else if (strcmp(query.values[ACTION], "clear-history") == 0) action = CALCULATOR_SESSION_CLEAR_HISTORY;
            else if (strcmp(query.values[ACTION], "release") == 0) action = CALCULATOR_SESSION_RELEASE;
            else { status = CALCULATOR_INVALID_ARGUMENT; goto done; }
            status = calculator_session_mutate(session, revision, action);
        } else if (strcmp(query.values[ACTION], "clear") == 0) {
            if (*body != '\0' || query.seen != (BIT(CLIENT) | BIT(ACTION) | BIT(REVISION))) { status = CALCULATOR_INVALID_ARGUMENT; goto done; }
            status = application_conversion_clear(session, revision);
        } else if (strcmp(query.values[ACTION], "commit") == 0) {
            char convert_target[512] = "/api/convert?";
            size_t used = strlen(convert_target);
            for (size_t i = FROM; i < QUERY_COUNT; i++) if (query.seen & BIT(i)) {
                int written = snprintf(convert_target + used, sizeof(convert_target) - used, "%s%s=%s", used == 13U ? "" : "&", keys[i], query.values[i]);
                if (written < 0 || (size_t)written >= sizeof(convert_target) - used) { status = CALCULATOR_INVALID_ARGUMENT; goto done; }
                used += (size_t)written;
            }
            NumForgeConversionOptions options;
            if (!numforge_web_parse_conversion_options(convert_target, &options)) { status = CALCULATOR_INVALID_ARGUMENT; goto done; }
            status = application_conversion_confirm(session, revision, body, options.from, options.to, &options.context, &confirmed, error, code);
        } else status = CALCULATOR_INVALID_ARGUMENT;
        if (status != CALCULATOR_OK) goto done;
    }
    append(&json, "{\"ok\":true,"); metadata(&json, session);
    if (confirmed != NULL) { append(&json, ",\"entry\":"); conversion(&json, confirmed, true); }
    else if (get && value) {
        const CalculatorValue *found = NULL;
        if (strcmp(query.values[NAME], "ans") == 0) found = calculator_session_answer(session);
        else for (size_t i = 0; i < session->variable_count; i++)
            if (strcmp(query.values[NAME], session->variables[i].name) == 0) { found = &session->variables[i].value; break; }
        if (found == NULL) { status = strcmp(query.values[NAME], "ans") == 0 ? CALCULATOR_UNDEFINED_ANSWER : CALCULATOR_UNDEFINED_VARIABLE; goto done; }
        append(&json, ",\"value\":"); snapshot(&json, found, full != 0U);
    } else if (get && (variables || history || conversions)) {
        size_t count = variables ? session->variable_count : history ? session->count : session->conversion_count;
        size_t begin = (size_t)offset, end = begin + (size_t)limit;
        if (end > count) end = count;
        if (query.seen & BIT(ID)) {
            begin = count;
            for (size_t i = 0; i < count; i++) {
                uint64_t candidate = history ? session->history[i].revision : session->conversions[i].revision;
                if (candidate == id) { begin = i; break; }
            }
            if (begin == count) { status = CALCULATOR_INVALID_ARGUMENT; goto done; }
            end = begin + 1U;
        }
        append(&json, ",\"total\":%zu,\"offset\":%zu,\"next_offset\":", count, begin);
        if (end < count) append(&json, "%zu", end); else append(&json, "null");
        append(&json, ",\"items\":[");
        for (size_t i = begin; i < end; i++) {
            if (i != begin) append(&json, ",");
            if (variables) {
                const CalculatorVariable *entry = &session->variables[i];
                char *display = NULL;
                status = calculator_format_value(&entry->value, &entry->value.context, &display);
                if (status != CALCULATOR_OK) { free(display); goto done; }
                append(&json, "{\"name\":"); string(&json, entry->name);
                append(&json, ",\"display\":"); string(&json, display); free(display);
                append(&json, ",\"value\":"); snapshot(&json, &entry->value, full != 0U); append(&json, "}");
            } else if (history) {
                const CalculatorHistoryEntry *entry = &session->history[i];
                append(&json, "{\"id\":\"%llu\",\"expression\":", (unsigned long long)entry->revision); string(&json, entry->expression);
                append(&json, ",\"display\":"); string(&json, entry->display);
                if (calculator_value_is_complex(&entry->value)) {
                    char *copy=NULL;
                    CalculatorStatus copied=calculator_value_complex_expression_text(&entry->value,&copy);
                    append(&json,",\"copy\":");
                    if (copied==CALCULATOR_OK) string(&json,copy); else append(&json,"null");
                    free(copy);
                }
                append(&json, ",\"value\":"); snapshot(&json, &entry->value, full != 0U); append(&json, "}");
            } else conversion(&json, &session->conversions[i], full != 0U);
        }
        append(&json, "]");
    }
    append(&json, "}");
done:
    if (status == CALCULATOR_OK) status = json.status;
    status = calculator_budget_status(status);
    if (status == CALCULATOR_OK) *response = json.data;
    else free(json.data);
    if (owner) numforge_budget_end();
    return status;
}

CalculatorStatus numforge_web_session_request(ApplicationClientStore *store,
    const char *method, const char *target, const char *body, char **response)
{
    CalculatorError error; calculator_error_clear(&error);
    if (response != NULL) *response = NULL;
    const char *code = "request_error";
    CalculatorStatus status = session_request(store, method, target, body, response, &error, &code);
    if (status != CALCULATOR_OK && response != NULL) {
        Json json = {numforge_malloc(RESPONSE_CAPACITY), 0, CALCULATOR_OK};
        if (json.data != NULL) {
            append(&json, "{\"ok\":false,\"schema_version\":1,\"status\":"); string(&json, calculator_status_to_string(status));
            append(&json, ",\"error\":"); string(&json, calculator_status_to_string(status));
            append(&json, ",\"code\":"); string(&json, code);
            append(&json, ",\"column\":%zu}", calculator_error_column(body == NULL ? "" : body, error.offset));
            *response = json.data;
        }
    }
    return status;
}
