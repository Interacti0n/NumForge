#include "apps/web/http/session_api.h"
#include "apps/web/http/unit_web.h"
#include "apps/web/http/web_api.h"
#include "src/engine/evaluator/functions.h"
#include "src/application/graphs/graph.h"
#include "src/application/graphs/fast_graph.h"
#include "src/application/graphs/graph_analysis.h"
#include "src/application/equations/equation.h"
#include "src/application/equations/numeric_root.h"
#include <errno.h>
#include <math.h>
#include "src/engine/api/status.h"
#include <numforge/runtime.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RESPONSE_CAPACITY (128U * 1024U)
#define GRAPH_RESPONSE_CAPACITY (256U * 1024U)
#define FAST_GRAPH_RESPONSE_CAPACITY (512U * 1024U)
typedef struct Json { char *data; size_t used; CalculatorStatus status; size_t capacity; } Json;

static void append(Json *json, const char *format, ...)
{
    if (json->status != CALCULATOR_OK) return;
    va_list args; va_start(args, format);
    int count = vsnprintf(json->data + json->used, json->capacity - json->used, format, args);
    va_end(args);
    if (count < 0 || (size_t)count >= json->capacity - json->used) json->status = CALCULATOR_VALUE_TOO_LARGE;
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
       FROM, TO, PRECISION, PLACES, ROUNDING, ANGLE, NOTATION,
       MINIMUM, MAXIMUM, POINTS, PARAMETER, MODE, WIDTH, HEIGHT, QUALITY, YMINIMUM, YMAXIMUM, INTERPOLATION, DENSE, RENDER, QUERY_COUNT };
static const char *keys[] = {"client", "action", "revision", "offset", "limit", "at", "id", "name", "full",
    "from", "to", "precision", "places", "rounding", "angle", "notation",
    "min", "max", "points", "parameter", "mode", "width", "height", "quality", "ymin", "ymax", "interpolation", "dense", "render"};
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
        strncmp(target, "/api/functions", 14U) == 0 || strncmp(target, "/api/graph", 10U) == 0 ||
        strncmp(target, "/api/equations", 14U) == 0;
}

static void plot_number(Json *json, const CalculatorValue *value, int places)
{
    if (json->status != CALCULATOR_OK) return;
    char *text = NULL;
    json->status = calculator_from_bigdecimal_status(bigdecimal_format_mode(value->number,
        places, BIGDECIMAL_ROUND_HALF_EVEN, BIGDECIMAL_FORMAT_SCIENTIFIC, 128U, &text));
    if (json->status == CALCULATOR_OK) string(json, text);
    free(text);
}

static bool camera_bound(const char *text, double *value)
{
    const char *p=text;
    if(*p=='+'||*p=='-')++p;
    bool digits=false;
    while(*p>='0'&&*p<='9'){digits=true;++p;}
    if(*p=='.'){++p;while(*p>='0'&&*p<='9'){digits=true;++p;}}
    if(!digits)return false;
    if(*p=='e'||*p=='E'){
        ++p;if(*p=='+'||*p=='-')++p;
        const char *start=p;while(*p>='0'&&*p<='9')++p;
        if(p==start)return false;
    }
    if(*p)return false;
    char *end=NULL;errno=0;*value=strtod(text,&end);
    return errno!=ERANGE&&isfinite(*value)&&end!=text&&*end=='\0';
}
static CalculatorStatus graph_request(ApplicationClientStore *store, const char *method,
    const Query *query, const char *body, char **response, CalculatorError *error, const char **code, bool adaptive)
{
    const unsigned int allowed = BIT(CLIENT) | BIT(AT) | BIT(MINIMUM) | BIT(MAXIMUM) |
        (adaptive ? BIT(MODE)|BIT(WIDTH)|BIT(HEIGHT)|BIT(QUALITY)|BIT(YMINIMUM)|BIT(YMAXIMUM)|BIT(INTERPOLATION)|BIT(DENSE)|BIT(RENDER) : BIT(POINTS)) | BIT(PARAMETER) | BIT(PRECISION) | BIT(ANGLE);
    uint64_t points, precision, at,width,height;
    if (strcmp(method, "POST") != 0 || (query->seen & ~allowed) ||
        !(query->seen & BIT(MINIMUM)) || !(query->seen & BIT(MAXIMUM)) ||
        !number(query, POINTS, 129U, &points) || points < 2U || points > APPLICATION_EXPRESSION_MAX_SAMPLES ||
        !number(query, PRECISION, CALCULATOR_DEFAULT_DIVISION_SCALE, &precision) ||
        precision == 0U || precision > CALCULATOR_MAX_OUTPUT_SCALE || !number(query, AT, 0U, &at) ||
        ((query->seen & BIT(AT)) && !(query->seen & BIT(CLIENT)))) return CALCULATOR_INVALID_ARGUMENT;
    bool fast_mode=(query->seen&BIT(MODE))!=0U;
    if(fast_mode&&strcmp(query->values[MODE],"fast")!=0)return CALCULATOR_INVALID_ARGUMENT;
    bool integer_points=(query->seen&BIT(RENDER))&&strcmp(query->values[RENDER],"points")==0;
    bool automatic_points=(query->seen&BIT(RENDER))&&strcmp(query->values[RENDER],"auto")==0;
    if((query->seen&BIT(RENDER))&&(!fast_mode||(!integer_points&&!automatic_points&&strcmp(query->values[RENDER],"curve")!=0)))return CALCULATOR_INVALID_ARGUMENT;
    if(!fast_mode&&(query->seen&(BIT(WIDTH)|BIT(HEIGHT)|BIT(QUALITY)|BIT(YMINIMUM)|BIT(YMAXIMUM)|BIT(INTERPOLATION)|BIT(DENSE))))return CALCULATOR_INVALID_ARGUMENT;
    bool subpixel=(query->seen&BIT(INTERPOLATION))&&strcmp(query->values[INTERPOLATION],"hermite-subpixel")==0;
    bool hermite=subpixel||((query->seen&BIT(INTERPOLATION))&&strcmp(query->values[INTERPOLATION],"hermite")==0);
    if((query->seen&BIT(INTERPOLATION))&&!hermite&&strcmp(query->values[INTERPOLATION],"linear")!=0)return CALCULATOR_INVALID_ARGUMENT;
    double vertical_span=0,camera_low=0,camera_high=0;
    bool dense=(query->seen&BIT(DENSE))!=0U;
    if(dense&&(strcmp(query->values[DENSE],"band")!=0||!(query->seen&BIT(YMINIMUM))||!(query->seen&BIT(YMAXIMUM))))return CALCULATOR_INVALID_ARGUMENT;
    if(hermite&&(!(query->seen&BIT(YMINIMUM))||!(query->seen&BIT(YMAXIMUM))))return CALCULATOR_INVALID_ARGUMENT;
    if(query->seen&(BIT(YMINIMUM)|BIT(YMAXIMUM))){
        if(!(query->seen&BIT(YMINIMUM))||!(query->seen&BIT(YMAXIMUM)))return CALCULATOR_INVALID_ARGUMENT;
        double low,high;
        if(!camera_bound(query->values[YMINIMUM],&low)||!camera_bound(query->values[YMAXIMUM],&high)||high<=low||!isfinite(high-low))return CALCULATOR_INVALID_ARGUMENT;
        vertical_span=high-low;
        camera_low=low;camera_high=high;
    }
    if(!number(query,WIDTH,800U,&width)||width<320U||width>2048U||!number(query,HEIGHT,440U,&height)||height<160U||height>1200U)return CALCULATOR_INVALID_ARGUMENT;
    bool coarse=(query->seen&BIT(QUALITY))&&strcmp(query->values[QUALITY],"coarse")==0;
    if((query->seen&BIT(QUALITY))&&!coarse&&strcmp(query->values[QUALITY],"refined")!=0)return CALCULATOR_INVALID_ARGUMENT;
    CalculatorContext context; calculator_context_init(&context);
    context.division_scale = (int64_t)precision;
    if (query->seen & BIT(ANGLE))
    {
        if (strcmp(query->values[ANGLE], "deg") == 0) context.angle_unit = CALCULATOR_ANGLE_DEGREES;
        else if (strcmp(query->values[ANGLE], "rad") != 0) return CALCULATOR_INVALID_ARGUMENT;
    }
    const CalculatorSession *session = NULL;
    if (query->seen & BIT(CLIENT))
    {
        if (strlen(query->values[CLIENT]) != 32U || strspn(query->values[CLIENT], "0123456789abcdef") != 32U)
            return CALCULATOR_INVALID_ARGUMENT;
        session = application_client_retained_session(store, query->values[CLIENT]);
        if (session == NULL || session->released) return CALCULATOR_SESSION_EXPIRED;
        if ((query->seen & BIT(AT)) && at != session->revision) return CALCULATOR_STALE_REQUEST;
    }
    *code = "graph_error";
    bool owner = numforge_budget_begin(CALCULATOR_DEFAULT_TIME_LIMIT_MS,
        CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    BigDecimal *decimal = bigdecimal_create(), *precision_y_minimum = NULL, *precision_y_maximum = NULL;
    BigRational *minimum = bigrational_create(), *maximum = bigrational_create();
    ApplicationGraph *graph = NULL;
    Json json = {NULL, 0U, CALCULATOR_OK, adaptive ? GRAPH_RESPONSE_CAPACITY : RESPONSE_CAPACITY};
    CalculatorStatus status = decimal == NULL || minimum == NULL || maximum == NULL ? CALCULATOR_OUT_OF_MEMORY : CALCULATOR_OK;
    if (status == CALCULATOR_OK) status = calculator_from_bigdecimal_status(bigdecimal_set_string(decimal, query->values[MINIMUM]));
    if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_from_bigdecimal(minimum, decimal));
    if (status == CALCULATOR_OK) status = calculator_from_bigdecimal_status(bigdecimal_set_string(decimal, query->values[MAXIMUM]));
    if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_from_bigdecimal(maximum, decimal));
    const char *parameter = query->seen & BIT(PARAMETER) ? query->values[PARAMETER] : adaptive ? NULL : "x";
    if(status==CALCULATOR_OK&&fast_mode&&(precision==CALCULATOR_DEFAULT_DIVISION_SCALE||integer_points||automatic_points)){
        char *end=NULL;errno=0;double low=strtod(query->values[MINIMUM],&end);
        bool usable=errno!=ERANGE&&isfinite(low)&&end!=query->values[MINIMUM]&&*end=='\0';
        errno=0;double high=strtod(query->values[MAXIMUM],&end);usable=usable&&errno!=ERANGE&&isfinite(high)&&end!=query->values[MAXIMUM]&&*end=='\0';
        ApplicationFastGraph *fast=NULL;
        status=CALCULATOR_NOT_IMPLEMENTED;
        if(usable&&high>low&&(integer_points||automatic_points))status=application_graph_integer_points(&fast,body,parameter,low,high,(size_t)width,automatic_points,session,&context,error);
        if(status==CALCULATOR_NOT_IMPLEMENTED&&precision==CALCULATOR_DEFAULT_DIVISION_SCALE&&usable&&high>low)status=vertical_span>0?
            (subpixel?application_fast_graph_sample_camera_hermite:application_fast_graph_sample_camera)(&fast,body,parameter,low,high,(size_t)width,(size_t)height,camera_low,camera_high,coarse,dense,session,&context,error):
            application_fast_graph_sample_view_dense(&fast,body,parameter,low,high,(size_t)width,(size_t)height,0,coarse,dense,session,&context,error);
        if(status==CALCULATOR_OK){
fast_response:
            json.capacity=FAST_GRAPH_RESPONSE_CAPACITY;json.data=numforge_malloc(json.capacity);
            if(!json.data)status=CALCULATOR_OUT_OF_MEMORY;
            else{
                append(&json,"{\"ok\":true,\"schema_version\":3,\"sampling\":\"%s\",\"evaluation\":\"%s\",\"parameter\":",fast->discrete?"discrete":"screen",fast->discrete?"precision":"double");string(&json,fast->parameter);
                if(fast->discrete)append(&json,",\"render\":\"points\",\"point_stride\":%.17g",fast->point_stride);
                append(&json,",\"quality\":\"%s\",\"width\":%llu,\"height\":%llu,\"limited\":%s,\"exact_probes\":%zu,\"y_min\":",coarse&&!fast->discrete?"coarse":"refined",(unsigned long long)width,(unsigned long long)height,fast->limited?"true":"false",fast->exact_probes);
                if(isfinite(fast->minimum))append(&json,"%.17g",fast->minimum);else append(&json,"null");
                append(&json,",\"y_max\":");if(isfinite(fast->maximum))append(&json,"%.17g",fast->maximum);else append(&json,"null");
                append(&json,",\"count\":%zu,\"plot_places\":16,\"points\":[",fast->count);
                for(size_t i=0;i<fast->count&&json.status==CALCULATOR_OK;++i){const ApplicationFastGraphPoint *p=&fast->points[i];
                    append(&json,"%s[%.17g,",i?",":"",p->x);if(isfinite(p->y))append(&json,"%.17g",p->y);else append(&json,"null");
                    append(&json,",%u,%u,%u]",p->connect?1U:0U,(unsigned)p->kind,p->approximate?1U:0U);
                }
                append(&json,"]");
                if(fast->screen_clipped)append(&json,",\"screen_clipped\":true");
                if(fast->band_count){
                    append(&json,",\"dense_representation\":\"pixel-envelope\",\"bands\":[");
                    for(size_t i=0;i<fast->band_count&&json.status==CALCULATOR_OK;++i){const ApplicationFastGraphBand *band=&fast->bands[i];
                        append(&json,"%s[%.17g,%.17g,%.17g,%.17g]",i?",":"",band->low,band->high,band->minimum,band->maximum);
                    }
                    append(&json,"]");
                }
                /* Optional slopes must not displace required points or exceed
                 * the existing response bound. Absence means linear fallback. */
                if(hermite&&!fast->discrete&&json.status==CALCULATOR_OK&&json.capacity-json.used>fast->count*18U+64U){
                    append(&json,",\"slopes\":[");
                    for(size_t i=0U;i<fast->count;++i){
                        if(isfinite(fast->points[i].derivative))append(&json,"%s%.9g",i?",":"",fast->points[i].derivative);
                        else append(&json,"%snull",i?",":"");
                    }
                    append(&json,"]");
                    if(fast->subpixel_hermite)append(&json,",\"subpixel_hermite\":true");
                }else if(fast->subpixel_hermite&&json.status==CALCULATOR_OK){
                    /* Optimized edges require slopes. If they cannot fit, use
                     * the legacy sampler within the same inherited budget. */
                    free(json.data);json.data=NULL;json.used=0;json.status=CALCULATOR_OK;
                    application_fast_graph_destroy(fast);fast=NULL;
                    status=application_fast_graph_sample_camera(&fast,body,parameter,low,high,(size_t)width,(size_t)height,camera_low,camera_high,coarse,dense,session,&context,error);
                    if(status==CALCULATOR_OK)goto fast_response;
                    goto graph_done;
                }
                append(&json,"}");status=json.status;
            }
            application_fast_graph_destroy(fast);goto graph_done;
        }
        application_fast_graph_destroy(fast);
        if(status==CALCULATOR_NOT_IMPLEMENTED)status=CALCULATOR_OK;
    }
    if (status == CALCULATOR_OK && adaptive && fast_mode && vertical_span > 0) {
        precision_y_minimum = bigdecimal_create();precision_y_maximum = bigdecimal_create();
        status = precision_y_minimum && precision_y_maximum ? CALCULATOR_OK : CALCULATOR_OUT_OF_MEMORY;
        if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_set_string(precision_y_minimum, query->values[YMINIMUM]));
        if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_set_string(precision_y_maximum, query->values[YMAXIMUM]));
        if (status == CALCULATOR_OK) status = application_graph_adaptive_camera(&graph, body, parameter, minimum, maximum,
            precision_y_minimum, precision_y_maximum, (size_t)width, (size_t)height, session, &context, error);
    } else if (status == CALCULATOR_OK) status = adaptive ?
        application_graph_adaptive(&graph, body, parameter, minimum, maximum, session, &context, error) :
        application_graph_sample(&graph, body, parameter, minimum, maximum, (size_t)points, session, &context, error);
    if (adaptive && parameter == NULL && status == CALCULATOR_UNDEFINED_VARIABLE) *code = "graph_parameter_ambiguous";
    if (status == CALCULATOR_OK && adaptive) parameter = graph->parameter;
    if (status == CALCULATOR_OK)
    {
        json.data = numforge_malloc(json.capacity);
        if (json.data == NULL) status = CALCULATOR_OUT_OF_MEMORY;
    }
    if (status == CALCULATOR_OK)
    {
        append(&json, "{\"ok\":true,\"schema_version\":%u,\"parameter\":", adaptive ? 2U : 1U); string(&json, parameter);
        if (adaptive) {
            append(&json, ",\"sampling\":\"adaptive\",\"evaluation\":\"precision\",\"limited\":%s,\"y_min\":", graph->limited ? "true" : "false");
            CalculatorValue view = {0};view.number = graph->view_minimum;
            if (view.number != NULL) plot_number(&json, &view,16);else append(&json,"null");
            append(&json, ",\"y_max\":");view.number = graph->view_maximum;
            if (view.number != NULL) plot_number(&json, &view,16);else append(&json,"null");
            if (precision_y_minimum) {
                append(&json, ",\"quality\":\"refined\",\"width\":%llu,\"height\":%llu", (unsigned long long)width, (unsigned long long)height);
                if (graph->screen_clipped) append(&json, ",\"screen_clipped\":true");
            }
        }
        append(&json, ",\"count\":%zu,\"plot_places\":%u,\"points\":[", graph->count,adaptive?16U:12U);
        static const char *kinds[] = {"real", "domain_error", "complex", "quantity"};
        for (size_t i = 0; i < graph->count && json.status == CALCULATOR_OK; ++i)
        {
            const ApplicationExpressionSample *sample = &graph->samples[i];
            ApplicationGraphPointKind kind = application_graph_point_kind(sample);
            append(&json, "%s{", i == 0U ? "" : ",");
            if (adaptive) append(&json, "\"connect\":%s,", graph->connect[i] ? "true" : "false");
            append(&json,"\"x\":"); plot_number(&json, &graph->coordinates[i],adaptive?16:12);
            append(&json, ",\"kind\":\"%s\",\"y\":", kinds[kind]);
            if (kind == APPLICATION_GRAPH_REAL) plot_number(&json, &sample->value,adaptive?16:12);
            else append(&json, "null");
            append(&json, ",\"source_approximate\":%s", sample->error.status == CALCULATOR_OK &&
                (sample->value.kind == CALCULATOR_VALUE_DECIMAL || sample->value.kind == CALCULATOR_VALUE_COMPLEX_DECIMAL)
                ? "true" : "false");
            if (kind == APPLICATION_GRAPH_DOMAIN_ERROR)
            {
                append(&json, ",\"status\":"); string(&json, calculator_status_to_string(sample->error.status));
                append(&json, ",\"column\":%zu", calculator_error_column(body, sample->error.offset));
            }
            append(&json, "}");
        }
        append(&json, "]}"); status = json.status;
    }
graph_done:
    status = calculator_budget_status(status);
    application_graph_destroy(graph); bigdecimal_destroy(decimal);
    bigdecimal_destroy(precision_y_minimum);bigdecimal_destroy(precision_y_maximum);
    bigrational_destroy(minimum); bigrational_destroy(maximum);
    if (status == CALCULATOR_OK) *response = json.data;
    else free(json.data);
    if (owner) numforge_budget_end();
    return status;
}

static CalculatorStatus graph_verify_request(const char *method,const Query *query,const char *body,char **response,CalculatorError *error,const char **code)
{
    *code="graph_verification_error";
    uint64_t precision=34U;
    if(strcmp(method,"POST")!=0||(query->seen&~BIT(PRECISION))||!number(query,PRECISION,34U,&precision)||precision<34U||precision>100U||strlen(body)>4096U)return CALCULATOR_INVALID_ARGUMENT;
    CalculatorContext context;calculator_context_init(&context);context.division_scale=(int64_t)precision;context.time_limit_ms=1000;
    bool owner=numforge_budget_begin(1000U,CALCULATOR_ALLOCATION_BUDGET,CALCULATOR_SINGLE_ALLOCATION);
    size_t length=strlen(body);char *copy=numforge_malloc(length+1U);CalculatorStatus status=copy?CALCULATOR_OK:CALCULATOR_OUT_OF_MEMORY;
    ApplicationGraphCandidate candidates[APPLICATION_GRAPH_MAX_CANDIDATES]={0};ApplicationGraphVerifiedPoint verified[APPLICATION_GRAPH_MAX_CANDIDATES]={0};
    size_t count=0U;bool limited=false;char *first=copy,*second=NULL,*row=NULL;
    if(copy){memcpy(copy,body,length+1U);second=strchr(copy,'\n');if(second){*second++='\0';row=strchr(second,'\n');if(row)*row++='\0';}if(!*first||!second||!row||!*row)status=CALCULATOR_INVALID_ARGUMENT;}
    while(status==CALCULATOR_OK&&row&&*row){
        if(count>=APPLICATION_GRAPH_MAX_CANDIDATES){status=CALCULATOR_INVALID_ARGUMENT;break;}
        char *next=strchr(row,'\n');if(next)*next++='\0';
        if(row[0]<'0'||row[0]>'3'||row[1]!=' '){status=CALCULATOR_INVALID_ARGUMENT;break;}
        ApplicationGraphCandidate *candidate=&candidates[count];candidate->kind=(ApplicationGraphCandidateKind)(row[0]-'0');char *token=row+2;
        double *values[]={&candidate->x,&candidate->low,&candidate->high,&candidate->x_tolerance,&candidate->y_tolerance};
        for(size_t i=0;i<5U;++i){char *end=strchr(token,' ');if((i<4U&&!end)||(i==4U&&end)){status=CALCULATOR_INVALID_ARGUMENT;break;}if(end)*end='\0';if(strlen(token)>=48U||!camera_bound(token,values[i])){status=CALCULATOR_INVALID_ARGUMENT;break;}if(end)token=end+1;}
        if(status==CALCULATOR_OK)++count;
        row=next;
    }
    ApplicationGraphVerificationStats stats={0};
    if(status==CALCULATOR_OK)status=application_graph_verify_profile(verified,&limited,first,second,candidates,count,&context,error,true,&stats);
    Json json={NULL,0U,CALCULATOR_OK,RESPONSE_CAPACITY};
    if(status==CALCULATOR_OK){json.data=numforge_malloc(json.capacity);if(!json.data)status=CALCULATOR_OUT_OF_MEMORY;}
    if(status==CALCULATOR_OK){
        append(&json,"{\"ok\":true,\"schema_version\":1,\"evaluation\":\"precision\",\"limited\":%s,\"count\":%zu,\"evaluations\":%zu,\"sample_requests\":%zu,\"cache_hits\":%zu,\"points\":[",limited?"true":"false",count,stats.evaluations,stats.sample_requests,stats.cache_hits);
        for(size_t i=0;i<count;++i){const ApplicationGraphVerifiedPoint *point=&verified[i];if(i)append(&json,",");
            if(!point->confirmed)append(&json,"null");else append(&json,"{\"x\":%.17g,\"y\":%.17g,\"residual\":%.17g,\"approximate\":%s}",point->x,point->y,point->residual,point->approximate?"true":"false");}
        append(&json,"],\"refinement_steps\":%zu,\"bisections\":%zu,\"interpolations\":%zu,\"confirmed\":%zu,\"non_real\":%zu,\"residual_rejections\":%zu,\"iteration_limits\":%zu,\"coordinate_limits\":%zu,\"unprocessed\":%zu}",stats.refinement_steps,stats.bisections,stats.interpolations,stats.confirmed,stats.non_real,stats.residual_rejections,stats.iteration_limits,stats.coordinate_limits,stats.unprocessed);status=json.status;
    }
    free(copy);status=calculator_budget_status(status);if(status==CALCULATOR_OK)*response=json.data;else free(json.data);if(owner)numforge_budget_end();return status;
}
static void equation_rational(Json *json, const BigRational *value)
{
    if (json->status != CALCULATOR_OK) return;
    if (!value) { append(json, "null"); return; }
    char *text = NULL;
    json->status = calculator_from_rational_status(bigrational_to_string(value, &text));
    if (json->status == CALCULATOR_OK) string(json, text);
    free(text);
}

static void numeric_decimal(Json *json, const BigDecimal *value)
{
    if (json->status != CALCULATOR_OK) return;
    char *text = NULL;
    json->status = calculator_from_decimal_status(bigdecimal_format_mode(value, -1,
        BIGDECIMAL_ROUND_HALF_EVEN, BIGDECIMAL_FORMAT_AUTO, CALCULATOR_MAX_OUTPUT_BYTES, &text));
    if (json->status == CALCULATOR_OK) string(json, text);
    free(text);
}
static void numeric_root_json(Json *json, const ApplicationNumericRoot *root,
    BigDecimal *decimal, BigDecimal *one, int64_t precision, bool rational_coordinate)
{
    if (json->status != CALCULATOR_OK) return;
    json->status = calculator_from_rational_status(bigrational_to_bigdecimal(decimal, numforge_root_x(root), precision, BIGDECIMAL_ROUND_HALF_EVEN));
    append(json, "{\"x\":");
    if (rational_coordinate) { equation_rational(json, numforge_root_x(root)); append(json, ",\"coordinate_kind\":\"rational\""); }
    else numeric_decimal(json, decimal);
    if (json->status == CALCULATOR_OK) json->status = calculator_from_decimal_status(bigdecimal_set_string(one, "1"));
    if (json->status == CALCULATOR_OK) json->status = calculator_from_decimal_status(bigdecimal_div_significant(decimal, decimal, one, precision, BIGDECIMAL_ROUND_HALF_EVEN));
    append(json, ",\"approximation\":"); numeric_decimal(json, decimal);
    append(json, ",\"residual\":"); numeric_decimal(json, numforge_root_residual(root));
    append(json, ",\"exact_zero\":%s}", numforge_root_exact_zero(root) ? "true" : "false");
}
static CalculatorStatus numeric_equation_request(const char *method, const Query *query,
    const char *body, char **response, CalculatorError *error, const char **code)
{
    *code = "numeric_options_error";
    uint64_t precision;
    if (strcmp(method, "POST") != 0 || (query->seen & ~(BIT(MODE)|BIT(PRECISION)|BIT(MINIMUM)|BIT(MAXIMUM))) ||
        !(query->seen & BIT(MINIMUM)) || !(query->seen & BIT(MAXIMUM)) ||
        !number(query, PRECISION, CALCULATOR_DEFAULT_DIVISION_SCALE, &precision) || precision < 16U || precision > 100U)
        return CALCULATOR_INVALID_ARGUMENT;
    *code = "numeric_equation_error";
    CalculatorContext context; calculator_context_init(&context); context.division_scale = (int64_t)precision;
    bool owner = numforge_budget_begin((uint64_t)context.time_limit_ms, CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    BigRational *bounds[2] = {bigrational_create(), bigrational_create()};
    BigDecimal *decimal = bigdecimal_create(), *one = bigdecimal_create();
    ApplicationNumericRoot *root = NULL; ApplicationRootScan *scan = NULL;
    bool scan_mode = strcmp(query->values[MODE], "numeric_scan") == 0;
    CalculatorStatus status = bounds[0] && bounds[1] && decimal && one ? CALCULATOR_OK : CALCULATOR_OUT_OF_MEMORY;
    const char *texts[] = {query->values[MINIMUM], query->values[MAXIMUM]};
    for (size_t i = 0U; i < 2U && status == CALCULATOR_OK; ++i) {
        status = calculator_from_decimal_status(bigdecimal_set_string(decimal, texts[i]));
        if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_from_bigdecimal(bounds[i], decimal));
    }
    int bound_order = 0;
    if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_compare(&bound_order, bounds[0], bounds[1]));
    if (status == CALCULATOR_OK && bound_order >= 0) status = CALCULATOR_INVALID_ARGUMENT;
    if (status == CALCULATOR_INVALID_ARGUMENT) *code = "numeric_options_error";
    if (status == CALCULATOR_OK) {
        *code = "numeric_equation_error";
        status = scan_mode ? application_numeric_roots_scan(&scan, body, bounds[0], bounds[1], &context, error) :
            application_numeric_root_solve(&root, body, bounds[0], bounds[1], &context, error);
    }
    Json json = {NULL, 0U, CALCULATOR_OK, RESPONSE_CAPACITY};
    if (status == CALCULATOR_OK) { json.data = numforge_malloc(json.capacity); if (!json.data) status = CALCULATOR_OUT_OF_MEMORY; }
    if (status == CALCULATOR_OK) {
        NumForgeRootScanStats scan_stats;
        numforge_root_scan_stats(scan, &scan_stats);
        if (scan_mode) append(&json, "{\"ok\":true,\"schema_version\":1,\"kind\":\"numeric_scan\",\"outcome\":\"%s\",\"complete\":false,\"count\":%zu,\"evaluations\":%zu,\"iterations\":%zu,\"segments\":%u,\"scanned_segments\":%zu,\"domain_gaps\":%zu,\"rejected_brackets\":%zu,\"max_roots\":%u,\"max_evaluations\":%u,\"x_tolerance\":\"1E-12\",\"residual_tolerance\":\"1E-12\",\"lower\":",
            application_numeric_roots_stop(numforge_root_scan_stop(scan)), numforge_root_scan_count(scan), scan_stats.evaluations, scan_stats.iterations,
            APPLICATION_ROOT_SCAN_SEGMENTS, scan_stats.scanned_segments, scan_stats.domain_gaps, scan_stats.rejected_brackets,
            APPLICATION_ROOT_SCAN_MAX_ROOTS, APPLICATION_ROOT_SCAN_MAX_EVALUATIONS);
        else append(&json, "{\"ok\":true,\"schema_version\":1,\"kind\":\"numeric\",\"outcome\":\"%s\",\"complete\":false,\"count\":%u,\"evaluations\":%zu,\"iterations\":%zu,\"x_tolerance\":\"1E-12\",\"residual_tolerance\":\"1E-12\",\"lower\":",
            application_numeric_root_kind(numforge_root_kind(root)), numforge_root_kind(root) == APPLICATION_ROOT_FOUND ? 1U : 0U, numforge_root_evaluations(root), numforge_root_iterations(root));
        equation_rational(&json, scan_mode ? bounds[0] : numforge_root_lower(root)); append(&json, ",\"upper\":"); equation_rational(&json, scan_mode ? bounds[1] : numforge_root_upper(root));
        if (scan_mode) append(&json, ",\"candidate_samples\":%zu,\"exact_candidate_roots\":%zu,\"candidate_gaps\":%zu", scan_stats.candidate_samples, scan_stats.candidate_roots, scan_stats.candidate_gaps);
        append(&json, ",\"roots\":[");
        if (scan_mode) for (size_t i = 0U; i < numforge_root_scan_count(scan) && json.status == CALCULATOR_OK; ++i) {
            if (i) append(&json, ",");
            numeric_root_json(&json, numforge_root_scan_root(scan, i), decimal, one, (int64_t)precision, true);
        }
        else if (numforge_root_kind(root) == APPLICATION_ROOT_FOUND) numeric_root_json(&json, root, decimal, one, (int64_t)precision, false);
        append(&json, "]}"); status = json.status;
    }
    status = calculator_budget_status(status);
    if (status == CALCULATOR_OK) *response = json.data;
    else { free(json.data); calculator_error_set(error, status, error ? error->offset : 0U); }
    bigrational_destroy(bounds[0]); bigrational_destroy(bounds[1]); bigdecimal_destroy(decimal); bigdecimal_destroy(one); application_numeric_root_destroy(root); application_numeric_roots_destroy(scan);
    if (owner) numforge_budget_end();
    return status;
}

static CalculatorStatus equation_request(const char *method, const Query *query,
    const char *body, char **response, CalculatorError *error, const char **code)
{
    if ((query->seen & BIT(MODE)) && (!strcmp(query->values[MODE], "numeric") || !strcmp(query->values[MODE], "numeric_scan")))
        return numeric_equation_request(method, query, body, response, error, code);
    uint64_t precision;
    *code = "equation_request_error";
    bool expression_mode = (query->seen & BIT(MODE)) && strcmp(query->values[MODE], "expression") == 0;
    size_t length = 0U;
    while (length <= CALCULATOR_MAX_INPUT_BYTES && body[length]) ++length;
    if (strcmp(method, "POST") != 0 || (query->seen & ~(BIT(PRECISION)|BIT(MODE))) ||
        ((query->seen & BIT(MODE)) && !expression_mode && strcmp(query->values[MODE], "coefficients") != 0) ||
        !number(query, PRECISION, CALCULATOR_DEFAULT_DIVISION_SCALE, &precision) ||
        precision < 1U || precision > CALCULATOR_MAX_OUTPUT_SCALE || length > CALCULATOR_MAX_INPUT_BYTES)
        return CALCULATOR_INVALID_ARGUMENT;
    CalculatorContext context;
    calculator_context_init(&context);
    context.division_scale = (int64_t)precision;
    bool owner = numforge_budget_begin((uint64_t)context.time_limit_ms,
        CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    char *copy = expression_mode ? NULL : numforge_malloc(length + 1U);
    CalculatorStatus status = expression_mode || copy ? CALCULATOR_OK : CALCULATOR_OUT_OF_MEMORY;
    const char *inputs[3] = {copy, NULL, NULL};
    size_t offsets[3] = {0U}, failed = SIZE_MAX;
    ApplicationEquationSolution *solution = NULL;
    Json json = {NULL, 0U, CALCULATOR_OK, RESPONSE_CAPACITY};
    if (copy) {
        memcpy(copy, body, length + 1U);
        for (size_t i = 1U; i < 3U; ++i) {
            char *separator = strchr(inputs[i-1U], '\n');
            if (!separator) { status = CALCULATOR_INVALID_ARGUMENT; break; }
            *separator = '\0';
            inputs[i] = separator + 1;
            offsets[i] = (size_t)(inputs[i] - copy);
        }
        if (status == CALCULATOR_OK && strchr(inputs[2], '\n')) status = CALCULATOR_INVALID_ARGUMENT;
    }
    if (status == CALCULATOR_OK)
        status = expression_mode ? application_equation_solve_text(&solution, body, &context, error) :
            application_equation_solve_expressions(&solution, inputs, &context, error, &failed);
    if (failed < 3U) {
        static const char *codes[] = {"coefficient_a_error", "coefficient_b_error", "coefficient_c_error"};
        *code = codes[failed];
        calculator_error_set(error, status, offsets[failed] + (error ? error->offset : 0U));
    } else if (status != CALCULATOR_OK) *code = expression_mode ? "equation_expression_error" : "equation_request_error";
    if (status == CALCULATOR_OK) {
        json.data = numforge_malloc(json.capacity);
        if (!json.data) status = CALCULATOR_OUT_OF_MEMORY;
    }
    if (status == CALCULATOR_OK) {
        const char *kind = numforge_polynomial_kind(solution) == APPLICATION_EQUATION_FINITE ? "finite" :
            numforge_polynomial_kind(solution) == APPLICATION_EQUATION_ALL ? "all" : "none";
        append(&json, "{\"ok\":true,\"schema_version\":1,\"kind\":\"%s\",\"degree\":%u,\"count\":%zu,\"imaginary\":%s,\"center\":",
            kind, numforge_polynomial_degree(solution), numforge_polynomial_root_count(solution), numforge_polynomial_is_imaginary(solution) ? "true" : "false");
        equation_rational(&json, numforge_polynomial_center(solution));
        append(&json, ",\"radicand\":"); equation_rational(&json, numforge_polynomial_radicand(solution));
        append(&json, ",\"offset\":"); equation_rational(&json, numforge_polynomial_rational_offset(solution));
        append(&json, ",\"discriminant\":"); equation_rational(&json, numforge_polynomial_discriminant(solution));
        append(&json, ",\"roots\":[");
        for (size_t i = 0U; i < numforge_polynomial_root_count(solution) && json.status == CALCULATOR_OK; ++i) {
            ApplicationEquationRootText *text = NULL;
            json.status = application_equation_format_root(&text, solution, i, &context, error);
            if (json.status == CALCULATOR_OK) {
                append(&json, "%s{\"branch\":\"%s\",\"multiplicity\":%u,\"exact\":", i ? "," : "",
                    numforge_polynomial_root_count(solution) == 1U ? "single" : i ? "plus" : "minus", numforge_polynomial_root_multiplicity(solution, i));
                string(&json, text->exact); append(&json, ",\"approximation\":"); string(&json, text->approximation); append(&json, "}");
            }
            application_equation_root_text_destroy(text);
        }
        append(&json, "]}");
        status = json.status;
    }
    status = calculator_budget_status(status);
    if (status == CALCULATOR_OK) *response = json.data;
    else { free(json.data); calculator_error_set(error, status, error ? error->offset : 0U); }
    application_equation_destroy(solution);
    free(copy);
    if (owner) numforge_budget_end();
    return status;
}

static CalculatorStatus session_request(ApplicationClientStore *store,
    const char *method, const char *target, const char *body, char **response,
    CalculatorError *error, const char **code)
{
    if (store == NULL || method == NULL || target == NULL || body == NULL || response == NULL) return CALCULATOR_NULL_ARGUMENT;
    *response = NULL;
    uint64_t conversion_started = UINT64_MAX;
    uint64_t conversion_finished = UINT64_MAX;
    Query query; char path[64];
    if (!parse(target, path, &query)) return CALCULATOR_INVALID_ARGUMENT;
    if (strcmp(path, "/api/equations/solve") == 0)
        return equation_request(method, &query, body, response, error, code);
    if(strcmp(path,"/api/graph/verify")==0)return graph_verify_request(method,&query,body,response,error,code);
    if (strcmp(path, "/api/graph/sample") == 0 || strcmp(path, "/api/graph/plot") == 0)
        return graph_request(store, method, &query, body, response, error, code, strcmp(path, "/api/graph/plot") == 0);
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
    Json json = {numforge_malloc(RESPONSE_CAPACITY), 0, CALCULATOR_OK, RESPONSE_CAPACITY};
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
            conversion_started = numforge_monotonic_ms();
            status = application_conversion_confirm(session, revision, body, options.from, options.to, &options.context, &confirmed, error, code);
            conversion_finished = numforge_monotonic_ms();
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
    if (status == CALCULATOR_OK) numforge_web_result_timing(response, conversion_started, conversion_finished);
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
        Json json = {numforge_malloc(RESPONSE_CAPACITY), 0, CALCULATOR_OK, RESPONSE_CAPACITY};
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
