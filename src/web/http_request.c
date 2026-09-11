#include "http_request.h"
#include "web_api.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

/* Socket-free request framing; all scans operate on a bounded local copy. */
static char numforge_ascii_lower(char character)
{
    return character >= 'A' && character <= 'Z'
        ? (char)(character + ('a' - 'A')) : character;
}

static bool numforge_ascii_equals(
    const char *name,
    size_t length,
    const char *expected
)
{
    size_t index;

    if (strlen(expected) != length)
    {
        return false;
    }
    for (index = 0U; index < length; index++)
    {
        if (numforge_ascii_lower(name[index]) != numforge_ascii_lower(expected[index]))
        {
            return false;
        }
    }
    return true;
}

static bool numforge_origin_matches(
    const char *origin,
    size_t origin_length,
    const char *host,
    uint16_t port
)
{
    char expected[64];
    int length;

    /* Browsers omit the default HTTP port when serializing an Origin. */
    if (port == 80U)
    {
        length = snprintf(expected, sizeof(expected), "http://%s", host);
        if (length > 0 && (size_t)length < sizeof(expected) &&
            numforge_ascii_equals(origin, origin_length, expected)) return true;
    }
    length = snprintf(expected, sizeof(expected), "http://%s:%u",
                      host, (unsigned int)port);

    return length > 0 && (size_t)length < sizeof(expected) &&
           numforge_ascii_equals(origin, origin_length, expected);
}

static bool numforge_parse_request_headers(
    const char *request,
    const char *header_end,
    unsigned long long *body_length,
    bool *content_length_present,
    bool *origin_allowed,
    uint16_t port
)
{
    const char *line = strstr(request, "\r\n");
    bool origin_present = false;

    *body_length = 0U;
    *content_length_present = false;
    *origin_allowed = true;
    if (line == NULL || line > header_end)
    {
        return false;
    }
    if (line == header_end)
    {
        return true;
    }
    line += 2;

    while (line < header_end)
    {
        const char *line_end = strstr(line, "\r\n");
        const char *colon;

        if (line_end == NULL || line_end > header_end)
        {
            return false;
        }
        colon = memchr(line, ':', (size_t)(line_end - line));
        if (colon == NULL)
        {
            return false;
        }

        if (numforge_ascii_equals(
                line, (size_t)(colon - line), "Content-Length"))
        {
            const char *value = colon + 1;
            unsigned long long parsed = 0U;

            if (*content_length_present)
            {
                return false;
            }
            while (value < line_end && (*value == ' ' || *value == '\t')) value++;
            if (value == line_end || *value < '0' || *value > '9')
            {
                return false;
            }
            while (value < line_end && *value >= '0' && *value <= '9')
            {
                unsigned int digit = (unsigned int)(*value - '0');

                if (parsed > (ULLONG_MAX - digit) / 10U)
                {
                    return false;
                }
                parsed = parsed * 10U + digit;
                value++;
            }
            while (value < line_end && (*value == ' ' || *value == '\t')) value++;
            if (value != line_end)
            {
                return false;
            }

            *body_length = parsed;
            *content_length_present = true;
        }
        else if (numforge_ascii_equals(line, (size_t)(colon - line), "Transfer-Encoding"))
        {
            /* Only Content-Length framing is supported; do not silently
             * interpret a chunked or ambiguous request as a plain body. */
            return false;
        }
        else if (numforge_ascii_equals(
                     line, (size_t)(colon - line), "Origin"))
        {
            const char *value = colon + 1;
            const char *value_end = line_end;
            size_t value_length;

            if (origin_present)
            {
                return false;
            }
            origin_present = true;
            while (value < line_end && (*value == ' ' || *value == '\t')) value++;
            while (value_end > value &&
                   (value_end[-1] == ' ' || value_end[-1] == '\t')) value_end--;
            value_length = (size_t)(value_end - value);
            *origin_allowed =
                numforge_origin_matches(value, value_length, "127.0.0.1", port) ||
                numforge_origin_matches(value, value_length, "localhost", port);
        }
        if (line_end == header_end)
        {
            return true;
        }
        line = line_end + 2;
    }

    return false;
}

bool numforge_request_target(const char *request, char *method, size_t method_capacity,
                                    char *target, size_t target_capacity)
{
    const char *line_end;
    const char *first_space;
    const char *second_space;
    const char *version;
    size_t method_length;
    size_t target_length;
    size_t version_length;

    if (request == NULL || method == NULL || target == NULL ||
        method_capacity == 0U || target_capacity == 0U)
    {
        return false;
    }

    line_end = strstr(request, "\r\n");
    if (line_end == NULL)
    {
        return false;
    }
    first_space = memchr(request, ' ', (size_t)(line_end - request));
    if (first_space == NULL || first_space == request)
    {
        return false;
    }
    second_space = memchr(first_space + 1, ' ', (size_t)(line_end - first_space - 1));
    if (second_space == NULL || second_space == first_space + 1)
    {
        return false;
    }

    method_length = (size_t)(first_space - request);
    target_length = (size_t)(second_space - first_space - 1);
    version = second_space + 1;
    version_length = (size_t)(line_end - version);
    if (method_length >= method_capacity || target_length >= target_capacity ||
        !((version_length == strlen("HTTP/1.0") &&
           memcmp(version, "HTTP/1.0", version_length) == 0) ||
          (version_length == strlen("HTTP/1.1") &&
           memcmp(version, "HTTP/1.1", version_length) == 0)))
    {
        return false;
    }

    memcpy(method, request, method_length);
    method[method_length] = '\0';
    memcpy(target, first_space + 1, target_length);
    target[target_length] = '\0';
    return true;
}


NumForgeHttpStatus numforge_http_probe(const char *data, size_t used, size_t capacity,
    uint16_t port, NumForgeHttpFrame *frame)
{
    char request[NUMFORGE_WEB_REQUEST_CAPACITY];
    const char *end;
    unsigned long long body_length;
    if (data == NULL || frame == NULL || capacity > sizeof(request) ||
        capacity < 1U || used >= capacity) return NUMFORGE_HTTP_BAD;
    memset(frame, 0, sizeof(*frame));
    memcpy(request, data, used);
    request[used] = '\0';
    end = strstr(request, "\r\n\r\n");
    if (end == NULL)
        return memchr(data, '\0', used) != NULL || used + 1U == capacity
            ? NUMFORGE_HTTP_BAD : NUMFORGE_HTTP_MORE;
    frame->header_length = (size_t)(end - request) + 4U;
    if (!numforge_request_target(request, frame->method, sizeof(frame->method),
                                frame->target, sizeof(frame->target)) ||
        !numforge_parse_request_headers(request, end, &body_length,
            &frame->has_content_length, &frame->origin_allowed, port))
        return NUMFORGE_HTTP_BAD;
    if (body_length > NUMFORGE_WEB_MAX_EXPRESSION_LENGTH ||
        body_length > capacity - frame->header_length - 1U) return NUMFORGE_HTTP_LARGE;
    frame->length = frame->header_length + (size_t)body_length;
    return used < frame->length ? NUMFORGE_HTTP_MORE : NUMFORGE_HTTP_READY;
}
