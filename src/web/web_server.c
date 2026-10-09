#include "../internal/benchmark_profile.h"
#include "../internal/numforge_alloc.h"
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include "web_server.h"
#include "web_api.h"
#include "client_store.h"
#include "unit_web.h"
#include "session_api.h"
#include "http_request.h"
#include "web_page.h"
#include <numforge/runtime.h>

#define NUMFORGE_WEB_SOCKET_TIMEOUT_MS 2000

/*
------------------------------------------------------------------------------------------------------------------------------
    Minimal loopback HTTP backend for local or proxied NumForge demonstrations. It is
    intentionally not an Internet-facing server: it accepts one request at a
    time, serves build-embedded calculator/help assets, and sends expressions to the C
    parser and typed evaluator through web_api.c.
------------------------------------------------------------------------------------------------------------------------------
*/

/*
------------------------------------------------------------------------------------------------------------------------------
    HTTP response and request parsing helpers.
------------------------------------------------------------------------------------------------------------------------------
*/

static bool numforge_socket_retryable(
    void
)
{
#ifdef _WIN32
    int error = WSAGetLastError();

    return error == WSAEWOULDBLOCK || error == WSAEINTR;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
#endif
}

static bool numforge_wait_socket(
    NumForgeSocket socket_value,
    bool writing,
    uint64_t deadline
)
{
    for (;;)
    {
        fd_set ready;
        struct timeval timeout;
        uint64_t now = numforge_monotonic_ms();
        uint64_t remaining;
        int selected;

        if (now == UINT64_MAX || now >= deadline)
        {
            return false;
        }

        remaining = deadline - now;
        timeout.tv_sec = (long)(remaining / 1000U);
        timeout.tv_usec = (long)(remaining % 1000U) * 1000L;
        FD_ZERO(&ready);
        FD_SET(socket_value, &ready);
#ifdef _WIN32
        selected = select(0, writing ? NULL : &ready, writing ? &ready : NULL, NULL, &timeout);

        if (selected < 0 && WSAGetLastError() == WSAEINTR)
        {
            continue;
        }
#else
        selected = select(socket_value + 1, writing ? NULL : &ready, writing ? &ready : NULL, NULL, &timeout);

        if (selected < 0 && errno == EINTR)
        {
            continue;
        }
#endif
        return selected > 0;
    }
}

static bool numforge_send_all(
    NumForgeSocket socket,
    const char *data,
    size_t length,
    uint64_t deadline
)
{
    while (length > 0U)
    {
        if (!numforge_wait_socket(socket, true, deadline))
        {
            return false;
        }

        int sent = send(socket, data, (int)(length > 32767U ? 32767U : length), 0);

        if (sent < 0 && numforge_socket_retryable())
        {
            continue;
        }

        if (sent <= 0)
        {
            return false;
        }

        data += (size_t)sent;
        length -= (size_t)sent;
    }

    return true;
}

static void numforge_send_bytes_response(
    NumForgeSocket socket,
    int status,
    const char *status_text,
    const char *content_type,
    const char *body,
    size_t body_length
)
{
    char header[256];
    uint64_t deadline = numforge_monotonic_ms() + NUMFORGE_WEB_SOCKET_TIMEOUT_MS;
    int length = snprintf(header,
                          sizeof(header),
                          "HTTP/1.1 %d %s\r\n"
                          "Content-Type: %s\r\n"
                          "Content-Length: %zu\r\n"
                          "Connection: close\r\n"
                          "Cache-Control: no-store\r\n\r\n",
                          status,
                          status_text,
                          content_type,
                          body_length);

    if (length > 0 && (size_t)length < sizeof(header))
    {
        if (numforge_send_all(socket, header, (size_t)length, deadline))
        {
            (void)numforge_send_all(socket, body, body_length, deadline);
        }
    }
}

static void numforge_send_response(
    NumForgeSocket socket,
    int status,
    const char *status_text,
    const char *content_type,
    const char *body
)
{
    NumForgeProfilePhase previous = numforge_profile_enter(NUMFORGE_PHASE_SEND);
    numforge_send_bytes_response(socket, status, status_text, content_type, body, strlen(body));
    numforge_profile_leave(previous);
}

static void numforge_send_page(
    NumForgeSocket socket,
    const char *const *parts
)
{
    char header[256];
    uint64_t deadline = numforge_monotonic_ms() + NUMFORGE_WEB_SOCKET_TIMEOUT_MS;
    size_t content_length = 0U;
    size_t index;
    int header_length;

    for (index = 0U; parts[index] != NULL; index++)
    {
        content_length += strlen(parts[index]);
    }

    header_length = snprintf(header,
                             sizeof(header),
                             "HTTP/1.1 200 OK\r\n"
                             "Content-Type: text/html; charset=utf-8\r\n"
                             "Content-Length: %zu\r\n"
                             "Connection: close\r\n"
                             "Cache-Control: no-store\r\n\r\n",
                             content_length);

    if (header_length <= 0 || (size_t)header_length >= sizeof(header))
    {
        return;
    }

    if (!numforge_send_all(socket, header, (size_t)header_length, deadline))
    {
        return;
    }

    for (index = 0U; parts[index] != NULL; index++)
    {
        if (!numforge_send_all(socket, parts[index], strlen(parts[index]), deadline))
        {
            return;
        }
    }
}

static const char *numforge_find_header_end(
    const char *request
)
{
    return strstr(request, "\r\n\r\n");
}

static bool numforge_read_request(
    NumForgeSocket socket,
    char *buffer,
    size_t capacity,
    size_t *length,
    bool *has_content_length,
    bool *origin_allowed,
    uint16_t port,
    const char *public_origin,
    int *http_error
)
{
    size_t used = 0U;
    uint64_t deadline = numforge_monotonic_ms() + NUMFORGE_WEB_SOCKET_TIMEOUT_MS;
    *http_error = 400;

    for (;;)
    {
        NumForgeHttpFrame frame;
        NumForgeHttpStatus status = numforge_http_probe_with_origin(buffer, used, capacity, port, public_origin, &frame);

        if (status == NUMFORGE_HTTP_READY)
        {
            *length = frame.length;
            *has_content_length = frame.has_content_length;
            *origin_allowed = frame.origin_allowed;
            buffer[*length] = '\0';

            return true;
        }

        if (status != NUMFORGE_HTTP_MORE)
        {
            *http_error = status == NUMFORGE_HTTP_LARGE ? 413 : 400;

            return false;
        }

        if (!numforge_wait_socket(socket, false, deadline))
        {
            *http_error = 408;

            return false;
        }

        int received = recv(socket, buffer + used, (int)(capacity - used - 1U), 0);

        if (received < 0 && numforge_socket_retryable())
        {
            continue;
        }

        if (received <= 0)
        {
            return false;
        }

        used += (size_t)received;
    }
}

static bool numforge_is_evaluation_target(
    const char *target
)
{
    static const char prefix[] = "/api/evaluate?precision=";

    return strcmp(target, "/api/evaluate") == 0 || strncmp(target, prefix, sizeof(prefix) - 1U) == 0;
}

static bool numforge_parse_evaluation_options(
    const char *target,
    int64_t *output_scale,
    CalculatorAngleUnit *angle_unit,
    CalculatorNotation *notation,
    BigComplexForm *form
)
{
    const char *value;
    const char *angle;
    const char *notation_option;
    size_t angle_length;
    char *end;
    long long parsed;
    char precision[32];
    size_t precision_length;
    char normalized[4096];
    *form=BIGCOMPLEX_FORM_CARTESIAN;
    const char *form_option=target == NULL ? NULL : strstr(target,"&form=");
    if (form_option != NULL) {
        const char *name=form_option+strlen("&form=");
        if (strcmp(name,"cartesian")==0) *form=BIGCOMPLEX_FORM_CARTESIAN;
        else if (strcmp(name,"trig")==0) *form=BIGCOMPLEX_FORM_TRIGONOMETRIC;
        else if (strcmp(name,"exp")==0) *form=BIGCOMPLEX_FORM_EXPONENTIAL;
        else return false;
        size_t length=(size_t)(form_option-target);
        if (length >= sizeof(normalized)) return false;
        memcpy(normalized,target,length);normalized[length]='\0';target=normalized;
    }

    if (target == NULL || output_scale == NULL || angle_unit == NULL || notation == NULL)
    {
        return false;
    }

    if (strcmp(target, "/api/evaluate") == 0)
    {
        *output_scale = CALCULATOR_DEFAULT_OUTPUT_SCALE;
        *angle_unit = CALCULATOR_ANGLE_RADIANS;
        *notation = CALCULATOR_NOTATION_AUTO;

        return true;
    }

    if (strncmp(target, "/api/evaluate?precision=", strlen("/api/evaluate?precision=")) != 0)
    {
        return false;
    }

    value = target + strlen("/api/evaluate?precision=");
    notation_option = strstr(value, "&notation=");
    *notation = CALCULATOR_NOTATION_AUTO;
    if (notation_option != NULL)
    {
        const char *name = notation_option + strlen("&notation=");
        if (strcmp(name, "auto") == 0) *notation = CALCULATOR_NOTATION_AUTO;
        else if (strcmp(name, "plain") == 0) *notation = CALCULATOR_NOTATION_PLAIN;
        else if (strcmp(name, "scientific") == 0) *notation = CALCULATOR_NOTATION_SCIENTIFIC;
        else if (strcmp(name, "math") == 0) *notation = CALCULATOR_NOTATION_MATHEMATICAL;
        else if (strcmp(name, "fraction") == 0) *notation = CALCULATOR_NOTATION_FRACTION;
        else return false;
    }
    angle = strstr(value, "&angle=");
    if (angle != NULL && notation_option != NULL && notation_option < angle)
    {
        return false;
    }
    precision_length = angle != NULL ? (size_t)(angle - value) :
        notation_option != NULL ? (size_t)(notation_option - value) : strlen(value);

    if (precision_length == 0U || precision_length >= sizeof(precision))
    {
        return false;
    }

    memcpy(precision, value, precision_length);
    precision[precision_length] = '\0';

    if (angle == NULL)
    {
        *angle_unit = CALCULATOR_ANGLE_RADIANS;
    }
    else
    {
        const char *name = angle + strlen("&angle=");
        angle_length = notation_option == NULL ? strlen(name) : (size_t)(notation_option - name);
        if (angle_length == 3U && strncmp(name, "rad", 3U) == 0)
        {
            *angle_unit = CALCULATOR_ANGLE_RADIANS;
        }
        else if (angle_length == 3U && strncmp(name, "deg", 3U) == 0)
        {
            *angle_unit = CALCULATOR_ANGLE_DEGREES;
        }
        else
        {
            return false;
        }
    }

    if (strcmp(precision, "full") == 0)
    {
        *output_scale = CALCULATOR_UNLIMITED_OUTPUT_SCALE;

        return true;
    }

    errno = 0;
    parsed = strtoll(precision, &end, 10);

    if (errno != 0 || end == precision || *end != '\0' || parsed < 0)
    {
        return false;
    }

    *output_scale = (int64_t)parsed;

    return true;
}

static bool numforge_parse_page_language(
    const char *target,
    const char *path,
    bool *english
)
{
    size_t path_length;

    if (target == NULL || path == NULL || english == NULL)
    {
        return false;
    }

    path_length = strlen(path);

    if (strcmp(target, path) == 0 ||
        (strncmp(target, path, path_length) == 0 && strcmp(target + path_length, "?lang=sk") == 0))
    {
        *english = false;

        return true;
    }

    if (strncmp(target, path, path_length) == 0 && strcmp(target + path_length, "?lang=en") == 0)
    {
        *english = true;

        return true;
    }

    return false;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Application-owned session/cache pools. HTTP borrows values; creation and FIFO eviction live in src/application/.
    Values were allocated under the calculator's single-allocation bound;
    numeric limb storage per retained rational-complex value is at most 512 KiB. No cookies or
    cross-tab storage: each page creates a fresh random identifier.
------------------------------------------------------------------------------------------------------------------------------
*/

static ApplicationClientStore numforge_application_clients;

static bool numforge_parse_session_action(char *target, const char **action)
{
    char *suffix = strstr(target, "&action=");

    *action = NULL;
    if (suffix == NULL)
    {
        return true;
    }
    *action = suffix + strlen("&action=");
    if (strcmp(*action, "start") != 0 && strcmp(*action, "preview") != 0 &&
        strcmp(*action, "commit") != 0 && strcmp(*action, "delete-variable") != 0)
    {
        return false;
    }
    *suffix = '\0';
    return true;
}

static bool numforge_parse_cache_options(char *target, char client[33], uint64_t *revision)
{
    char *suffix = strstr(target, "&client=");
    const char *number;
    uint64_t parsed = 0U;

    client[0] = '\0';
    *revision = 0U;
    if (suffix == NULL)
    {
        return true;
    }
    number = suffix + strlen("&client=");
    if (strlen(number) < 32U + strlen("&revision=") + 1U)
    {
        return false;
    }
    for (size_t index = 0U; index < 32U; index++)
    {
        if (!((number[index] >= '0' && number[index] <= '9') ||
              (number[index] >= 'a' && number[index] <= 'f')))
        {
            return false;
        }
    }
    if (strncmp(number + 32U, "&revision=", strlen("&revision=")) != 0)
    {
        return false;
    }
    memcpy(client, number, 32U);
    client[32] = '\0';
    number += 32U + strlen("&revision=");
    while (*number != '\0')
    {
        if (*number < '0' || *number > '9' ||
            parsed > (UINT64_C(9007199254740991) - (uint64_t)(*number - '0')) / 10U)
        {
            return false;
        }
        parsed = parsed * 10U + (uint64_t)(*number - '0');
        number++;
    }
    if (parsed == 0U)
    {
        return false;
    }
    *revision = parsed;
    *suffix = '\0';
    return true;
}

static char *numforge_math_copy_text(const char *display)
{
    static const char marker[] = " × 10^";
    const char *position = strstr(display, marker);
    const char *exponent;
    char *copy;
    char *end;
    size_t prefix;
    size_t length;

    if (position == NULL)
    {
        length = strlen(display);
        if (length > CALCULATOR_MAX_INPUT_BYTES) return NULL;
        copy = malloc(length + 1U);
        if (copy != NULL) memcpy(copy, display, length + 1U);
        return copy;
    }
    exponent = position + strlen(marker);
    errno = 0;
    if (strtoull(exponent + (*exponent == '+' || *exponent == '-'), &end, 10) >
            (unsigned long long)INT64_MAX ||
        errno != 0 || *end != '\0')
    {
        return NULL;
    }
    prefix = (size_t)(position - display);
    length = prefix + 1U + strlen(exponent) +
        ((*exponent == '+' || *exponent == '-') ? 0U : 1U);
    if (length > CALCULATOR_MAX_INPUT_BYTES) return NULL;
    copy = malloc(length + 1U);
    if (copy != NULL)
    {
        memcpy(copy, display, prefix);
        copy[prefix] = 'E';
        if (*exponent != '+' && *exponent != '-')
        {
            copy[prefix + 1U] = '+';
            memcpy(copy + prefix + 2U, exponent, strlen(exponent) + 1U);
        }
        else
        {
            memcpy(copy + prefix + 1U, exponent, strlen(exponent) + 1U);
        }
    }
    return copy;
}

static void numforge_handle_evaluation_impl(
    NumForgeSocket socket,
    const char *body,
    int64_t output_scale,
    CalculatorAngleUnit angle_unit,
    CalculatorNotation notation,
    BigComplexForm form,
    const char *client,
    uint64_t revision,
    const char *action
)
{
    CalculatorError error;
    CalculatorStatus status;
    char *result = NULL;
    char *response;
    char *copy_text = NULL;
    char *approximation = NULL;
    size_t response_capacity;
    bool reused = false;
    const CalculatorValue *output_value = NULL;

    if (action != NULL)
    {
        CalculatorSession *session = application_client_session(&numforge_application_clients, client, strcmp(action, "start") == 0);

        if (strcmp(action, "start") == 0)
        {
            if (session == NULL) {
                numforge_send_response(socket, 400, "Bad Request", "application/json; charset=utf-8",
                    "{\"ok\":false,\"status\":\"session expired; reload the page\",\"error\":\"session expired; reload the page\"}");
                return;
            }
            numforge_send_response(socket, 200, "OK", "application/json; charset=utf-8",
                "{\"ok\":true,\"result\":\"\"}");
            return;
        }
        if (session == NULL)
        {
            status = CALCULATOR_SESSION_EXPIRED;
            calculator_error_set(&error, status, 0U);
        }
        else if (strcmp(action, "delete-variable") == 0)
        {
            status = calculator_session_delete_variable(session, revision, body, &error);
            if (status == CALCULATOR_OK) {
                numforge_send_response(socket, 200, "OK", "application/json; charset=utf-8",
                    "{\"ok\":true,\"result\":\"\"}");
                return;
            }
        }
        else
        {
            status = numforge_web_evaluate_session_form(session, revision, strcmp(action, "commit") == 0,
                body, output_scale, angle_unit, notation, form, &result, &error, &reused);
            if (status == CALCULATOR_OK) output_value = strcmp(action,"commit")==0 ||
                strcmp(session->preview_expression,body)!=0 ? calculator_session_answer(session) : &session->preview;
        }
    }
    else
    {
        ApplicationEvaluationCache *cache=application_client_cache(&numforge_application_clients, client);
        status = numforge_web_evaluate_cached_form(
            cache, revision, body, output_scale, angle_unit,
            notation, form, &result, &error, &reused);
        if (status == CALCULATOR_OK && cache != NULL && strcmp(cache->expression,body)==0) output_value = &cache->value;
    }

    NumForgeProfilePhase serial_previous = numforge_profile_enter(NUMFORGE_PHASE_SERIALIZE);
    if (status == CALCULATOR_OK)
    {
        approximation = numforge_web_fraction_approximation(result);
        bool complex_output=calculator_value_is_complex(output_value);
        if (complex_output) {
            (void)calculator_value_complex_expression_text(output_value,&copy_text);
        } else if (notation == CALCULATOR_NOTATION_MATHEMATICAL)
        {
            copy_text = numforge_math_copy_text(result);
        }
        response_capacity = strlen(result) + (copy_text == NULL ? 0U : strlen(copy_text)) +
            (approximation == NULL ? 0U : strlen(approximation)) + 128U;
        response = malloc(response_capacity);

        if (response != NULL)
        {
            if (notation == CALCULATOR_NOTATION_MATHEMATICAL || complex_output)
            {
                if (*client == '\0')
                {
                    (void)snprintf(response, response_capacity,
                        "{\"ok\":true,\"result\":\"%s\",\"copy\":%s%s%s}",
                        result, copy_text == NULL ? "null" : "\"",
                        copy_text == NULL ? "" : copy_text, copy_text == NULL ? "" : "\"");
                }
                else
                {
                    (void)snprintf(response, response_capacity,
                        "{\"ok\":true,\"result\":\"%s\",\"copy\":%s%s%s,\"cached\":%s}",
                        result, copy_text == NULL ? "null" : "\"",
                        copy_text == NULL ? "" : copy_text, copy_text == NULL ? "" : "\"",
                        reused ? "true" : "false");
                }
            }
            else if (*client == '\0')
            {
                (void)snprintf(response, response_capacity, "{\"ok\":true,\"result\":\"%s\"}", result);
            }
            else
            {
                (void)snprintf(response, response_capacity, "{\"ok\":true,\"result\":\"%s\",\"cached\":%s}",
                               result, reused ? "true" : "false");
            }
            if (approximation != NULL)
            {
                size_t used = strlen(response);
                (void)snprintf(response + used - 1U, response_capacity - used + 1U,
                    ",\"approx\":\"%s\"}", approximation);
            }
            numforge_send_response(socket, 200, "OK", "application/json; charset=utf-8", response);
            free(response);
        }
        else
        {
            numforge_send_response(
                socket,
                500,
                "Internal Server Error",
                "application/json; charset=utf-8",
                "{\"ok\":false,\"error\":\"out of memory\",\"status\":\"out of memory\",\"column\":1}");
        }

        free(copy_text);
        free(approximation);

        free(result);
        numforge_profile_leave(serial_previous);

        return;
    }

    {
        char error_response[256];
        const char *status_text = calculator_status_to_string(status);
        size_t column = calculator_error_column(body, error.offset);
        int response_length =
            snprintf(error_response,
                     sizeof(error_response),
                     "{\"ok\":false,\"error\":\"%s at column %zu\",\"status\":\"%s\",\"column\":%zu}",
                     status_text,
                     column,
                     status_text,
                     column);

        if (response_length > 0 && (size_t)response_length < sizeof(error_response))
        {
            int http_status = status == CALCULATOR_OUT_OF_MEMORY ? 500 : 400;
            const char *http_status_text = http_status == 500 ? "Internal Server Error" : "Bad Request";

            numforge_send_response(
                socket, http_status, http_status_text, "application/json; charset=utf-8", error_response);
        }
        else
        {
            numforge_send_response(
                socket,
                500,
                "Internal Server Error",
                "application/json; charset=utf-8",
                "{\"ok\":false,\"error\":\"failed to format error response\",\"status\":\"out of memory\",\"column\":1}");
        }
    }
    numforge_profile_leave(serial_previous);
}

static void numforge_handle_evaluation(
    NumForgeSocket socket,
    const char *body,
    int64_t output_scale,
    CalculatorAngleUnit angle_unit,
    CalculatorNotation notation,
    BigComplexForm form,
    const char *client,
    uint64_t revision,
    const char *action
)
{
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    const char *trace = getenv("NUMFORGE_BENCH_TRACE");
    bool measure = trace != NULL && strcmp(trace, "1") == 0;
    if (measure)
    {
        const char *memory = getenv("NUMFORGE_BENCH_MEMORY");
        if (memory != NULL && strcmp(memory, "1") == 0)
            (void)numforge_alloc_stats_track(true);
        numforge_alloc_stats_reset();
        numforge_profile_reset();
    }
#endif
    numforge_handle_evaluation_impl(socket, body, output_scale, angle_unit,
        notation, form, client, revision, action);
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    if (measure)
    {
        numforge_profile_stop();
        fprintf(stderr, "BENCH,%d,%zu,%zu,%zu,%zu,%llu",
            numforge_profile_complete() && numforge_alloc_stats_complete(),
            numforge_alloc_stats_calls(), numforge_alloc_stats_bytes(),
            numforge_alloc_stats_live(), numforge_alloc_stats_peak(),
            numforge_profile_process_peak());
        for (int phase = 0; phase < NUMFORGE_PHASE_COUNT; phase++)
            fprintf(stderr, ",%.9f", numforge_profile_seconds((NumForgeProfilePhase)phase));
        fputc('\n', stderr);
        fflush(stderr);
    }
#endif
}

/* Read-only conversion never creates or mutates calculator sessions. */
static void numforge_handle_unit_catalog(NumForgeSocket socket)
{
    char *response = NULL;
    CalculatorStatus status = numforge_web_unit_catalog(&response);
    if (status == CALCULATOR_OK)
        numforge_send_response(socket, 200, "OK", "application/json; charset=utf-8", response);
    else numforge_send_response(socket, 500, "Internal Server Error", "application/json; charset=utf-8",
        "{\"ok\":false,\"code\":\"catalog_unavailable\",\"status\":\"out of memory\"}");
    free(response);
}
static void numforge_handle_conversion(NumForgeSocket socket, const char *target,
    const char *body, size_t body_length)
{
    NumForgeConversionOptions options;
    CalculatorError error;
    calculator_error_clear(&error);
    const char *code = "invalid_options";
    CalculatorStatus status = CALCULATOR_INVALID_ARGUMENT;
    char *response = NULL;
    if (memchr(body, '\0', body_length) != NULL) code = "invalid_body";
    else if (numforge_web_parse_conversion_options(target, &options))
    {
        CalculatorSession *session = options.client[0] ? application_client_session(&numforge_application_clients, options.client, false) : NULL;
        if (options.client[0] && session == NULL)
        {
            status = CALCULATOR_SESSION_EXPIRED;
            code = "session_expired";
        }
        else status = numforge_web_convert(session, body, &options, &response, &error, &code);
    }
    if (status == CALCULATOR_OK)
        numforge_send_response(socket, 200, "OK", "application/json; charset=utf-8", response);
    else
    {
        char failure[320];
        const char *message = numforge_web_conversion_error_message(code, status);
        snprintf(failure, sizeof(failure),
            "{\"ok\":false,\"code\":\"%s\",\"error\":\"%s\",\"status\":\"%s\",\"column\":%zu}",
            code, message, calculator_status_to_string(status), calculator_error_column(body, error.offset));
        int http_status = status == CALCULATOR_OUT_OF_MEMORY ? 500 : 400;
        numforge_send_response(socket, http_status, http_status == 500 ? "Internal Server Error" : "Bad Request",
            "application/json; charset=utf-8", failure);
    }
    free(response);
}

void numforge_handle_connection_with_origin(
    NumForgeSocket socket,
    uint16_t port,
    const char *public_origin
)
{
    char request[NUMFORGE_WEB_REQUEST_CAPACITY];
    char method[16];
    char target[512];
    char client[33];
    uint64_t revision;
    const char *action;
    const char *body;
    size_t length;
    size_t body_length = 0U;
    int64_t output_scale;
    CalculatorAngleUnit angle_unit;
    CalculatorNotation notation;
    BigComplexForm complex_form;
    bool english;
    bool has_content_length;
    bool origin_allowed;
    int http_error = 400;

    if (!numforge_read_request(socket,
                               request,
                               sizeof(request),
                               &length,
                               &has_content_length,
                               &origin_allowed,
                               port,
                               public_origin,
                               &http_error) ||
        !numforge_request_target(request, method, sizeof(method), target, sizeof(target)))
    {
        const char *reason =
            http_error == 413 ? "Payload Too Large" : (http_error == 408 ? "Request Timeout" : "Bad Request");
        const char *body_text =
            http_error == 413
                ? "{\"ok\":false,\"error\":\"request exceeds 4096 bytes\",\"status\":\"value too large\",\"column\":1}"
                : (http_error == 408
                       ? "{\"ok\":false,\"error\":\"request timeout\"}"
                       : "{\"ok\":false,\"error\":\"Bad request\",\"status\":\"invalid argument\",\"column\":1}");
        numforge_send_response(socket, http_error, reason, "application/json; charset=utf-8", body_text);

        if (http_error == 413)
        {
            /* Drain a bounded remainder so normal oversized requests can read
             * the response instead of receiving a reset on close. */
            char discarded[1024];
            size_t drained = 0U;
            uint64_t deadline = numforge_monotonic_ms() + 200U;

            while (drained < NUMFORGE_WEB_REQUEST_CAPACITY && numforge_wait_socket(socket, false, deadline))
            {
                int received = recv(socket, discarded, sizeof(discarded), 0);

                if (received <= 0)
                {
                    break;
                }

                drained += (size_t)received;
            }
        }

        return;
    }

    body = numforge_find_header_end(request);

    if (body != NULL)
    {
        body += 4;
        body_length = length - (size_t)(body - request);
    }

    if (strcmp(method, "GET") == 0 && numforge_web_is_session_target(target))
    {
        char *response = NULL;
        CalculatorStatus status = body_length != 0U ? CALCULATOR_INVALID_ARGUMENT :
            numforge_web_session_request(&numforge_application_clients, method, target, "", &response);
        if (status == CALCULATOR_OK)
            numforge_send_response(socket, 200, "OK", "application/json; charset=utf-8", response);
        else {
            char failure[512];
            snprintf(failure, sizeof(failure), "{\"ok\":false,\"status\":\"%s\",\"error\":\"%s\",\"column\":1}",
                calculator_status_to_string(status), calculator_status_to_string(status));
            numforge_send_response(socket, status == CALCULATOR_STALE_REQUEST ? 409 : 400,
                status == CALCULATOR_STALE_REQUEST ? "Conflict" : "Bad Request", "application/json; charset=utf-8", response != NULL ? response : failure);
        }
        free(response);
    }
    else if (strcmp(method, "GET") == 0 && strcmp(target, "/api/units") == 0)
    {
        numforge_handle_unit_catalog(socket);
    }
    else if (strcmp(method, "GET") == 0 && numforge_parse_page_language(target, "/", &english))
    {
        numforge_send_page(socket, english ? NUMFORGE_WEB_PAGE_EN : NUMFORGE_WEB_PAGE);
    }
    else if (strcmp(method, "GET") == 0 && numforge_parse_page_language(target, "/api", &english))
    {
        numforge_send_page(socket, english ? NUMFORGE_API_PAGE_EN : NUMFORGE_API_PAGE);
    }
    else if (strcmp(method, "GET") == 0 && numforge_parse_page_language(target, "/units", &english))
    {
        numforge_send_page(socket, english ? NUMFORGE_UNITS_PAGE_EN : NUMFORGE_UNITS_PAGE);
    }
    else if (strcmp(method, "GET") == 0 &&
             (numforge_parse_page_language(target, "/graph", &english) ||
              numforge_parse_page_language(target, "/solve", &english) ||
              numforge_parse_page_language(target, "/login", &english) ||
              numforge_parse_page_language(target, "/register", &english)))
    {
        numforge_send_page(socket, english ? NUMFORGE_UPCOMING_PAGE_EN : NUMFORGE_UPCOMING_PAGE);
    }
    else if (strcmp(method, "GET") == 0 && strcmp(target, "/assets/units.css") == 0)
    {
        numforge_send_response(socket, 200, "OK", "text/css; charset=utf-8", (const char *)NUMFORGE_UNITS_CSS);
    }
    else if (strcmp(method, "GET") == 0 && strcmp(target, "/assets/units.js") == 0)
    {
        numforge_send_response(socket, 200, "OK", "text/javascript; charset=utf-8", (const char *)NUMFORGE_UNITS_JS);
    }
    else if (strcmp(method, "GET") == 0 && strcmp(target, "/assets/calculator.css") == 0)
    {
        numforge_send_response(socket, 200, "OK", "text/css; charset=utf-8", (const char *)NUMFORGE_CALCULATOR_CSS);
    }
    else if (strcmp(method, "GET") == 0 && strcmp(target, "/assets/api.css") == 0)
    {
        numforge_send_response(socket, 200, "OK", "text/css; charset=utf-8", (const char *)NUMFORGE_API_CSS);
    }
    else if (strcmp(method, "GET") == 0 && strcmp(target, "/assets/chrome.css") == 0)
    {
        numforge_send_response(socket, 200, "OK", "text/css; charset=utf-8", (const char *)NUMFORGE_CHROME_CSS);
    }
    else if (strcmp(method, "GET") == 0 && strcmp(target, "/assets/calculator.js") == 0)
    {
        numforge_send_response(socket, 200, "OK", "text/javascript; charset=utf-8", (const char *)NUMFORGE_CALCULATOR_JS);
    }
    else if (strcmp(method, "GET") == 0 && strcmp(target, "/assets/navigation.js") == 0)
    {
        numforge_send_response(socket, 200, "OK", "text/javascript; charset=utf-8", (const char *)NUMFORGE_NAVIGATION_JS);
    }
    else if (strcmp(method, "GET") == 0 && strcmp(target, "/assets/logo.png") == 0)
    {
        numforge_send_bytes_response(socket, 200, "OK", "image/png",
                                     (const char *)NUMFORGE_LOGO_PNG, sizeof(NUMFORGE_LOGO_PNG) - 1U);
    }
    else if (strcmp(method, "GET") == 0 && strcmp(target, "/assets/wordmark.png") == 0)
    {
        numforge_send_bytes_response(socket, 200, "OK", "image/png",
                                     (const char *)NUMFORGE_WORDMARK_PNG, sizeof(NUMFORGE_WORDMARK_PNG) - 1U);
    }
    else if (strcmp(method, "GET") == 0 && strcmp(target, "/LICENSE") == 0)
    {
        numforge_send_response(socket, 200, "OK", "text/plain; charset=utf-8", (const char *)NUMFORGE_LICENSE_TEXT);
    }
    else if (strcmp(method, "POST") == 0 && !origin_allowed)
    {
        numforge_send_response(
            socket,
            403,
            "Forbidden",
            "application/json; charset=utf-8",
            "{\"ok\":false,\"error\":\"forbidden origin\",\"status\":\"invalid argument\",\"column\":1}");
    }
    else if (strcmp(method, "POST") == 0 && !has_content_length)
    {
        numforge_send_response(
            socket, 411, "Length Required", "text/plain; charset=utf-8", "Content-Length is required.\n");
    }
    else if (strcmp(method, "POST") == 0 && body != NULL &&
             numforge_web_is_session_target(target))
    {
        char *response = NULL;
        CalculatorStatus status = memchr(body, '\0', body_length) != NULL ? CALCULATOR_INVALID_ARGUMENT :
            numforge_web_session_request(&numforge_application_clients, method, target, body, &response);
        if (status == CALCULATOR_OK)
            numforge_send_response(socket, 200, "OK", "application/json; charset=utf-8", response);
        else {
            char failure[512];
            snprintf(failure, sizeof(failure), "{\"ok\":false,\"status\":\"%s\",\"error\":\"%s\",\"column\":1}",
                calculator_status_to_string(status), calculator_status_to_string(status));
            numforge_send_response(socket, status == CALCULATOR_STALE_REQUEST ? 409 : 400,
                status == CALCULATOR_STALE_REQUEST ? "Conflict" : "Bad Request", "application/json; charset=utf-8", response != NULL ? response : failure);
        }
        free(response);
    }
    else if (strcmp(method, "POST") == 0 && body != NULL &&
             (strcmp(target, "/api/convert") == 0 || strncmp(target, "/api/convert?", 13) == 0))
    {
        numforge_handle_conversion(socket, target, body, body_length);
    }
    else if (strcmp(method, "POST") == 0 && body != NULL && numforge_is_evaluation_target(target))
    {
        if (memchr(body, '\0', body_length) != NULL)
        {
            numforge_send_response(
                socket,
                400,
                "Bad Request",
                "application/json; charset=utf-8",
                "{\"ok\":false,\"error\":\"request body contains a NUL byte\",\"status\":\"invalid argument\",\"column\":1}");
        }
        else if (numforge_parse_session_action(target, &action) &&
                 numforge_parse_cache_options(target, client, &revision) &&
                 numforge_parse_evaluation_options(target, &output_scale, &angle_unit, &notation, &complex_form) &&
                 (action == NULL || *client != '\0'))
        {
            numforge_handle_evaluation(socket, body, output_scale, angle_unit, notation, complex_form, client, revision, action);
        }
        else
        {
            numforge_send_response(
                socket,
                400,
                "Bad Request",
                "application/json; charset=utf-8",
                "{\"ok\":false,\"error\":\"invalid precision, angle unit or notation\",\"status\":\"invalid argument\",\"column\":1}");
        }
    }
    else
    {
        numforge_send_response(socket, 404, "Not Found", "text/plain; charset=utf-8", "Not found.\n");
    }
}

void numforge_handle_connection(NumForgeSocket socket, uint16_t port)
{
    numforge_handle_connection_with_origin(socket, port, NULL);
}
